#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_TRANSPORT_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_TRANSPORT_H_

#include <grpc/support/log.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/core/ext/transport/binder/utils/transport_stream_receiver_interface.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/server_binder.h"
#include "src/core/ext/transport/binder/wire_format/wire_writer.h"
#include "src/core/lib/transport/transport.h"
#include "src/core/lib/transport/transport_impl.h"

// TODO(mingcl): Consider putting the struct in a namespace (Eventually this
// depends on what style we want to follow)
// TODO(mingcl): Decide casing for this class name. Should we use C-style class
// name here or just go with C++ style?
struct grpc_binder_transport {
  explicit grpc_binder_transport(
      std::unique_ptr<binder_transport::Binder> endpoint_binder);

  ~grpc_binder_transport();

  int NewStreamTxCode() {
    // TODO(mingcl): Wrap around when all tx codes are used. "If we do detect a
    // collision however, we will fail the new call with UNAVAILABLE, and shut
    // down the transport gracefully."
    GPR_ASSERT(next_free_tx_code <= LAST_CALL_TRANSACTION);
    return next_free_tx_code++;
  }

  grpc_transport base; /* must be first */

  std::unique_ptr<binder_transport::TransportStreamReceiverInterface>
      transport_stream_receiver;
  std::unique_ptr<binder_transport::WireWriter> wire_writer;
  std::unique_ptr<binder_transport::ServerBinder> server_binder;
  std::unique_ptr<binder_transport::TransactionReceiver> tx_receiver;

 private:
  int next_free_tx_code = binder_transport::kFirstCallId;
};

// Currently we always assume the transport is for client
grpc_transport* grpc_create_binder_transport(
    std::unique_ptr<binder_transport::Binder> endpoint_binder);

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_TRANSPORT_H_
