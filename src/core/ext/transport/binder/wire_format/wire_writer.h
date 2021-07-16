#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H_

#include <memory>
#include <utility>

#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/server_binder.h"

namespace binder_transport {

class WireWriter {
 public:
  virtual ~WireWriter() = default;
  virtual std::pair<std::unique_ptr<ServerBinder>,
                    std::unique_ptr<TransactionReceiver>>
  SetupTransport(std::unique_ptr<Binder> endpoint_binder) = 0;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_WIRE_WRITER_H_
