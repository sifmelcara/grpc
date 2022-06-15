// Copyright 2021 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpc/support/port_platform.h>

#include "src/core/ext/transport/binder/wire_format/wire_writer.h"

#ifndef GRPC_NO_BINDER

#include <utility>

#include "absl/cleanup/cleanup.h"

#include <grpc/support/log.h>

#define RETURN_IF_ERROR(expr)           \
  do {                                  \
    const absl::Status status = (expr); \
    if (!status.ok()) return status;    \
  } while (0)

namespace grpc_binder {

struct WireWriterImpl::RunScheduledTxArgs {
  WireWriterImpl* writer;
  std::unique_ptr<Transaction> tx;
  // How many data in transaction's `data` field has been sent.
  int64_t bytes_sent = 0;

  // TODO(unknown): union?
  bool is_ack_tx = false;
  int64_t ack_num_bytes;
};

bool CanBeSentInOneTransaction(const Transaction& tx) {
  return (tx.GetFlags() & kFlagMessageData) == 0 ||
         static_cast<int64_t>(tx.GetMessageData().size()) <=
             WireWriterImpl::kBlockSize;
}

// Simply forward the call to `WireWriterImpl::RunScheduledTx`
void RunScheduledTx(void* arg, grpc_error_handle /*error*/) {
  auto* run_scheduled_tx_args =
      static_cast<WireWriterImpl::RunScheduledTxArgs*>(arg);
  run_scheduled_tx_args->writer->RunScheduledTxInternal(run_scheduled_tx_args);
}

absl::Status WriteInitialMetadata(const Transaction& tx,
                                  WritableParcel* parcel) {
  if (tx.IsClient()) {
    // Only client sends method ref.
    RETURN_IF_ERROR(parcel->WriteString(tx.GetMethodRef()));
  }
  RETURN_IF_ERROR(parcel->WriteInt32(tx.GetPrefixMetadata().size()));
  for (const auto& md : tx.GetPrefixMetadata()) {
    RETURN_IF_ERROR(parcel->WriteByteArrayWithLength(md.first));
    RETURN_IF_ERROR(parcel->WriteByteArrayWithLength(md.second));
  }
  return absl::OkStatus();
}

absl::Status WriteTrailingMetadata(const Transaction& tx,
                                   WritableParcel* parcel) {
  if (tx.IsServer()) {
    if (tx.GetFlags() & kFlagStatusDescription) {
      RETURN_IF_ERROR(parcel->WriteString(tx.GetStatusDesc()));
    }
    RETURN_IF_ERROR(parcel->WriteInt32(tx.GetSuffixMetadata().size()));
    for (const auto& md : tx.GetSuffixMetadata()) {
      RETURN_IF_ERROR(parcel->WriteByteArrayWithLength(md.first));
      RETURN_IF_ERROR(parcel->WriteByteArrayWithLength(md.second));
    }
  } else {
    // client suffix currently is always empty according to the wireformat
    if (!tx.GetSuffixMetadata().empty()) {
      gpr_log(GPR_ERROR, "Got non-empty suffix metadata from client.");
    }
  }
  return absl::OkStatus();
}

WireWriterImpl::WireWriterImpl(std::unique_ptr<Binder> binder)
    : binder_(std::move(binder)), combiner_(grpc_combiner_create()) {
  gpr_log(GPR_INFO, "%s mu_ = %p , ack_mu_ = %p", __func__, &mu_, &ack_mu_);
}

WireWriterImpl::~WireWriterImpl() {
  GRPC_COMBINER_UNREF(combiner_, "wire_writer_impl");
}

// Flow control constant are specified at
// https://github.com/grpc/proposal/blob/master/L73-java-binderchannel/wireformat.md#flow-control
const int64_t WireWriterImpl::kBlockSize = 16 * 1024;
const int64_t WireWriterImpl::kFlowControlWindowSize = 128 * 1024;

absl::Status WireWriterImpl::MakeBinderTransaction(
    BinderTransportTxCode tx_code,
    std::function<absl::Status(WritableParcel*)> fill_parcel) {
  grpc_core::MutexLock lock(&mu_);
  RETURN_IF_ERROR(binder_->PrepareTransaction());
  WritableParcel* parcel = binder_->GetWritableParcel();
  RETURN_IF_ERROR(fill_parcel(parcel));
  // Only stream transaction is accounted in flow control spec.
  if (tx_code.code >= static_cast<int64_t>(kFirstCallId)) {
    num_outgoing_bytes_ += parcel->GetDataSize();
    gpr_log(GPR_INFO, "Total outgoing bytes: %ld", num_outgoing_bytes_.load());
  }
  GPR_ASSERT(!is_transacting_);
  is_transacting_ = true;
  absl::Status result = binder_->Transact(tx_code);
  is_transacting_ = false;
  return result;
}

absl::Status WireWriterImpl::RpcCallFastPath(std::unique_ptr<Transaction> tx) {
  return MakeBinderTransaction(
      BinderTransportTxCode(tx->GetTxCode()),
      [this, tx = tx.get()](
          WritableParcel* parcel) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
        RETURN_IF_ERROR(parcel->WriteInt32(tx->GetFlags()));
        RETURN_IF_ERROR(parcel->WriteInt32(next_seq_num_[tx->GetTxCode()]++));
        if (tx->GetFlags() & kFlagPrefix) {
          RETURN_IF_ERROR(WriteInitialMetadata(*tx, parcel));
        }
        if (tx->GetFlags() & kFlagMessageData) {
          RETURN_IF_ERROR(
              parcel->WriteByteArrayWithLength(tx->GetMessageData()));
        }
        if (tx->GetFlags() & kFlagSuffix) {
          RETURN_IF_ERROR(WriteTrailingMetadata(*tx, parcel));
        }
        return absl::OkStatus();
      });
}

// Slow path: the message data is too large to fit in one transaction.
absl::Status WireWriterImpl::RunChunkedTx(RunScheduledTxArgs* args,
                                          WritableParcel* parcel,
                                          bool* is_last_chunk) {
  auto tx = args->tx.get();
  // Transaction without data flag should go to fast path.
  GPR_ASSERT(tx->GetFlags() & kFlagMessageData);
  absl::string_view data = args->tx->GetMessageData();
  int flags = kFlagMessageData;
  if (args->bytes_sent == 0) {
    // This is the first transaction. Include initial
    // metadata if there's any.
    if (tx->GetFlags() & kFlagPrefix) {
      flags |= kFlagPrefix;
    }
  }
  // There is also prefix/suffix in transaction beside the transaction data so
  // actual transaction size will be greater than `kBlockSize`. It should be
  // fine because single tx size is not required to be 100% accurate and can be
  // a little bit off.
  int64_t size = std::min<int64_t>(WireWriterImpl::kBlockSize,
                                   data.size() - args->bytes_sent);
  GPR_ASSERT(args->bytes_sent <= static_cast<int64_t>(data.size()));
  if (args->bytes_sent + WireWriterImpl::kBlockSize >=
      static_cast<int64_t>(data.size())) {
    // This is the last transaction. Include trailing
    // metadata if there's any.
    if (tx->GetFlags() & kFlagSuffix) {
      flags |= kFlagSuffix;
    }
    size = data.size() - args->bytes_sent;
  } else {
    // There are more messages to send.
    flags |= kFlagMessageDataIsPartial;
    *is_last_chunk = false;
  }
  RETURN_IF_ERROR(parcel->WriteInt32(flags));
  RETURN_IF_ERROR(parcel->WriteInt32(next_seq_num_[tx->GetTxCode()]++));
  if (flags & kFlagPrefix) {
    RETURN_IF_ERROR(WriteInitialMetadata(*tx, parcel));
  }
  RETURN_IF_ERROR(
      parcel->WriteByteArrayWithLength(data.substr(args->bytes_sent, size)));
  if (flags & kFlagSuffix) {
    RETURN_IF_ERROR(WriteTrailingMetadata(*tx, parcel));
  }
  args->bytes_sent += size;
  return absl::OkStatus();
}

void WireWriterImpl::RunScheduledTxInternal(RunScheduledTxArgs* args) {
  GPR_ASSERT(args->writer == this);
  if (args->is_ack_tx) {
    absl::Status result = MakeBinderTransaction(
        ACKNOWLEDGE_BYTES, [args](WritableParcel* parcel) {
          RETURN_IF_ERROR(parcel->WriteInt64(args->ack_num_bytes));
          return absl::OkStatus();
        });
    if (!result.ok()) {
      gpr_log(GPR_ERROR, "Failed to make binder transaction %s",
              result.ToString().c_str());
    }
    delete args;
    return;
  }
  // Be reservative. Decrease CombinerTxCount after the data size of this
  // transaction has already been added to `num_outgoing_bytes_`, to make sure
  // we never underestimate `num_outgoing_bytes_`.
  auto decrease_combiner_tx_count = absl::MakeCleanup([this]() {
    {
      grpc_core::MutexLock lock(&ack_mu_);
      GPR_ASSERT(num_non_ack_tx_in_combiner_ > 0);
      num_non_ack_tx_in_combiner_--;
    }
    // New transaction might be ready to be scheduled.
    TryScheduleTransaction();
  });
  if (CanBeSentInOneTransaction(*args->tx)) {
    absl::Status result = RpcCallFastPath(std::move(args->tx));
    if (!result.ok()) {
      gpr_log(GPR_ERROR, "Failed to handle non-chunked RPC call %s",
              result.ToString().c_str());
    }
    delete args;
    return;
  }
  bool is_last_chunk = true;
  absl::Status result = MakeBinderTransaction(
      BinderTransportTxCode(args->tx->GetTxCode()),
      [args, &is_last_chunk, this](WritableParcel* parcel)
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
            return RunChunkedTx(args, parcel, &is_last_chunk);
          });
  if (!result.ok()) {
    gpr_log(GPR_ERROR, "Failed to make binder transaction %s",
            result.ToString().c_str());
  }
  if (!is_last_chunk) {
    auto next = new RunScheduledTxArgs();
    next->writer = args->writer;
    next->tx = std::move(args->tx);
    next->is_ack_tx = false;
    next->bytes_sent = args->bytes_sent;
    {
      grpc_core::MutexLock lock(&ack_mu_);
      pending_out_tx_.push(GRPC_CLOSURE_CREATE(RunScheduledTx, next, nullptr));
    }
    TryScheduleTransaction();
  }
  delete args;
}

absl::Status WireWriterImpl::RpcCall(std::unique_ptr<Transaction> tx) {
  // TODO(mingcl): check tx_code <= last call id
  GPR_ASSERT(tx->GetTxCode() >= kFirstCallId);
  auto args = new RunScheduledTxArgs();
  args->is_ack_tx = false;
  args->writer = this;
  args->tx = std::move(tx);
  {
    grpc_core::MutexLock lock(&ack_mu_);
    pending_out_tx_.push(GRPC_CLOSURE_CREATE(RunScheduledTx, args, nullptr));
  }
  TryScheduleTransaction();
  return absl::OkStatus();
}

absl::Status WireWriterImpl::SendAck(int64_t num_bytes) {
  gpr_log(GPR_INFO, "Ack %ld bytes received", num_bytes);
  if (is_transacting_) {
    // This can happen because NDK might call our registered callback function
    // while we are telling it to send a transaction, in the same call stack.
    gpr_log(GPR_INFO,
            "We are currently in the call stack of other transaction, "
            "scheduling it instead avoid deadlock.");
    auto args = new RunScheduledTxArgs();
    args->is_ack_tx = true;
    args->ack_num_bytes = num_bytes;
    args->writer = this;
    auto cl = GRPC_CLOSURE_CREATE(RunScheduledTx, args, nullptr);
    combiner_->Run(cl, GRPC_ERROR_NONE);
    return absl::OkStatus();
  }
  // Otherwise, we can simply directly send ack.
  absl::Status result = MakeBinderTransaction(
      ACKNOWLEDGE_BYTES, [num_bytes](WritableParcel* parcel) {
        RETURN_IF_ERROR(parcel->WriteInt64(num_bytes));
        return absl::OkStatus();
      });
  if (!result.ok()) {
    gpr_log(GPR_ERROR, "Failed to make binder transaction %s",
            result.ToString().c_str());
  }
  return result;
}

void WireWriterImpl::OnAckReceived(int64_t num_bytes) {
  gpr_log(GPR_INFO, "OnAckReceived %ld", num_bytes);
  // DO NOT try to obtain `mu_` in this codepath! NDKBinder might call back to
  // us when we are sending transaction
  {
    grpc_core::MutexLock lock(&ack_mu_);
    num_acknowledged_bytes_ = std::max(num_acknowledged_bytes_, num_bytes);
    int64_t num_outgoing_bytes = num_outgoing_bytes_;
    if (num_acknowledged_bytes_ > num_outgoing_bytes) {
      gpr_log(GPR_ERROR,
              "The other end of transport acked more bytes than we ever sent, "
              "%ld > %ld",
              num_acknowledged_bytes_, num_outgoing_bytes);
    }
  }
  TryScheduleTransaction();
}

// TODO(unknown): proof liveness?
// 1. Prove that if
//      a. OnAckReceived will be called for every 16KB
//      b. After RpcCall, the tasks in combiner will be run.
//    Then all message in RpcCall will be write to NdkBinder.
// 2. Prove that for every OnAckReceived call, the ack will be write to
// NdkBinder regardless of what WireWriterImpl's state is or what
// WireWriterImpl's interfaces are called.
//
// TODO(unknown): Document what kind of marginal error in flow control window is
// allowed.

void WireWriterImpl::TryScheduleTransaction() {
  while (true) {
    grpc_core::MutexLock lock(&ack_mu_);
    // TODO(unknown): explain: If "number of bytes already scheduled" - "acked
    // bytes" Number of bytes eventually will be inside NDK buffer assuming all
    // tasks in combiner will be scheduled and there is no new ACK.
    int64_t num_bytes_in_ndk_buffer =
        (num_outgoing_bytes_ + num_non_ack_tx_in_combiner_ * kBlockSize) -
        num_acknowledged_bytes_;
    if (num_bytes_in_ndk_buffer < 0) {
      gpr_log(GPR_ERROR,
              "Something went wrong. `num_bytes_in_ndk_buffer` should be "
              "non-negative but it is %ld",
              num_bytes_in_ndk_buffer);
    }
    if (!pending_out_tx_.empty() &&
        (num_bytes_in_ndk_buffer + kBlockSize < kFlowControlWindowSize)) {
      combiner_->Run(pending_out_tx_.front(), GRPC_ERROR_NONE);
      pending_out_tx_.pop();
      num_non_ack_tx_in_combiner_++;
    } else {
      break;
    }
  }
}

}  // namespace grpc_binder

#endif
