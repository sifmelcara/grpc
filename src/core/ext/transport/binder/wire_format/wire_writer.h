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

struct RunChunkedTxArgs;

class WireWriterImpl : public WireWriter {
 public:
  explicit WireWriterImpl(std::unique_ptr<Binder> binder);
  ~WireWriterImpl() override;
  absl::Status RpcCall(std::unique_ptr<Transaction> call) override;
  absl::Status SendAck(int64_t num_bytes) override;
  absl::Status RpcCallLocked(const Transaction& tx);
  absl::Status SendAckLocked(int64_t num_bytes);
  void OnAckReceived(int64_t num_bytes) override;

  // Only called in wire_writer.cc
  void AddPendingTx(grpc_closure* closure);
  void DecreaseCombinerTxCount();
  void TryScheduleTransaction();
  int num_tx_in_combiner_ = 0;
  // Fast path: send data in one transaction.
  absl::Status RpcCallFastPath(std::unique_ptr<Transaction> tx);

  absl::Status MakeTransaction(
      BinderTransportTxCode tx_code,
      std::function<absl::Status(WritableParcel*)> fill_parcel);
  absl::Status WriteInitialMetadata(const Transaction& tx,
                                    WritableParcel* parcel)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  absl::Status WriteTrailingMetadata(const Transaction& tx,
                                     WritableParcel* parcel)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  absl::Status WriteChunkedTx(RunChunkedTxArgs* args, WritableParcel* parcel,
                              bool* is_last_chunk);

  // Split long message into chunks of size 16k. This doesn't necessarily have
  // to be the same as the flow control acknowledgement size, but it should not
  // exceed 128k.
  static const int64_t kBlockSize;

  // Flow control allows sending at most 128k between acknowledgements.
  static const int64_t kFlowControlWindowSize;

  absl::flat_hash_map<int, int> seq_num_ ABSL_GUARDED_BY(mu_);

 private:
  grpc_core::Mutex mu_;
  std::unique_ptr<Binder> binder_ ABSL_GUARDED_BY(mu_);
  std::atomic_int64_t num_outgoing_bytes_{0};

  grpc_core::Mutex ack_mu_;
  int64_t num_acknowledged_bytes_ ABSL_GUARDED_BY(ack_mu_) = 0;
  int64_t num_scheduled_outgoing_bytes_ ABSL_GUARDED_BY(ack_mu_) = 0;

  grpc_core::Combiner* combiner_;
  std::queue<grpc_closure*> pending_out_tx_ ABSL_GUARDED_BY(ack_mu_);
};

}  // namespace grpc_binder

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H
