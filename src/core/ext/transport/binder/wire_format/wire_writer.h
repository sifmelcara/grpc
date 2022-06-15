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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H

#include <grpc/support/port_platform.h>

#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"

#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/transaction.h"
#include "src/core/lib/gprpp/sync.h"
#include "src/core/lib/iomgr/combiner.h"

namespace grpc_binder {

// Member functions are thread safe.
class WireWriter {
 public:
  virtual ~WireWriter() = default;
  virtual absl::Status RpcCall(std::unique_ptr<Transaction> call) = 0;
  virtual absl::Status SendAck(int64_t num_bytes) = 0;
  virtual void OnAckReceived(int64_t num_bytes) = 0;
};

class WireWriterImpl : public WireWriter {
 public:
  explicit WireWriterImpl(std::unique_ptr<Binder> binder);
  ~WireWriterImpl() override;
  absl::Status RpcCall(std::unique_ptr<Transaction> call) override;
  absl::Status SendAck(int64_t num_bytes) override;
  void OnAckReceived(int64_t num_bytes) override;

  // Required to be public because we would like to call this in combiner.
  // Should not be called by user directly.
  struct RunScheduledTxArgs;
  void RunScheduledTxInternal(RunScheduledTxArgs* arg);

  // Split long message into chunks of size 16k. This doesn't necessarily have
  // to be the same as the flow control acknowledgement size, but it should not
  // exceed 128k.
  static const int64_t kBlockSize;

  // Flow control allows sending at most 128k between acknowledgements.
  static const int64_t kFlowControlWindowSize;

 private:
  void TryScheduleTransaction();
  // Fast path: send data in one transaction.
  absl::Status RpcCallFastPath(std::unique_ptr<Transaction> tx);

  // This function will acquire `mu_` to make sure the binder is not used
  // concurrently, so this can be called by different threads safely.
  absl::Status MakeBinderTransaction(
      BinderTransportTxCode tx_code,
      std::function<absl::Status(WritableParcel*)> fill_parcel);

  absl::Status RunChunkedTx(RunScheduledTxArgs* args, WritableParcel* parcel,
                            bool* is_last_chunk)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  grpc_core::Mutex mu_;
  std::unique_ptr<Binder> binder_ ABSL_GUARDED_BY(mu_);

  // Maps the transaction code (which identifies streams) to their next
  // available sequence number. See
  // https://github.com/grpc/proposal/blob/master/L73-java-binderchannel/wireformat.md#sequence-number
  absl::flat_hash_map<int, int> next_seq_num_ ABSL_GUARDED_BY(mu_);

  std::atomic_int64_t num_outgoing_bytes_{0};

  grpc_core::Mutex ack_mu_;
  int64_t num_acknowledged_bytes_ ABSL_GUARDED_BY(ack_mu_) = 0;
  std::queue<grpc_closure*> pending_out_tx_ ABSL_GUARDED_BY(ack_mu_);
  int num_non_ack_tx_in_combiner_ ABSL_GUARDED_BY(ack_mu_) = 0;

  std::atomic_bool is_transacting_{false};

  grpc_core::Combiner* combiner_;
};

}  // namespace grpc_binder

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H
