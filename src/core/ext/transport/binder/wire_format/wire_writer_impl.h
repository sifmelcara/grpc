#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_IMPL_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_IMPL_H_

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/notification.h"
#include "src/core/ext/transport/binder/utils/transport_stream_receiver_interface.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/server_binder.h"
#include "src/core/ext/transport/binder/wire_format/wire_writer.h"

namespace binder_transport {

class WireWriterImpl : public WireWriter {
 public:
  explicit WireWriterImpl(binder_transport::TransportStreamReceiverInterface*
                              transport_stream_receiver);
  // TODO(mingcl): move dtor to .cc
  ~WireWriterImpl() override = default;
  std::pair<std::unique_ptr<ServerBinder>, std::unique_ptr<TransactionReceiver>>
  SetupTransport(std::unique_ptr<Binder> endpoint_binder) override;

  binder_status_t ProcessTransaction(transaction_code_t code,
                                     const OutputParcelInterface* output);

 private:
  TransportStreamReceiverInterface* transport_stream_receiver_;
  absl::Notification server_binder_noti_;
  std::unique_ptr<Binder> server_binder_;
  absl::flat_hash_map<transaction_code_t, uint32_t> expected_seq_num_;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_IMPL_H_
