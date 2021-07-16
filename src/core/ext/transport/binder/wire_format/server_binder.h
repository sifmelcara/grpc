#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_SERVER_BINDER_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_SERVER_BINDER_H_

#include <grpc/support/log.h>

#include <string>
#include <vector>

#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/transaction.h"

namespace binder_transport {

class ServerBinder {
 public:
  virtual ~ServerBinder() = default;
  virtual void RpcCall(const Transaction& call) = 0;
};

class ServerBinderImpl : public ServerBinder {
 public:
  explicit ServerBinderImpl(std::unique_ptr<Binder> binder);
  void RpcCall(const Transaction& tx) override;

 private:
  std::unique_ptr<Binder> binder_;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_SERVER_BINDER_H_
