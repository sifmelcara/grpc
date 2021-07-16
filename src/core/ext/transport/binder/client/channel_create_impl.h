#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_IMPL_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_IMPL_H_

#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/lib/channel/channel_args.h"

namespace grpc {
namespace internal {

grpc_channel* CreateChannelFromBinderImpl(
    std::unique_ptr<binder_transport::Binder> endpoint_binder,
    const grpc_channel_args* args);

}  // namespace internal
}  // namespace grpc

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_IMPL_H_
