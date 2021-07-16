#include "src/core/ext/transport/binder/client/channel_create_impl.h"

#include <memory>
#include <utility>

#include "src/core/ext/transport/binder/transport/binder_transport.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/lib/channel/channel_args.h"
#include "src/core/lib/surface/api_trace.h"
#include "src/core/lib/surface/channel.h"

namespace grpc {
namespace internal {

grpc_channel* CreateChannelFromBinderImpl(
    std::unique_ptr<binder_transport::Binder> endpoint_binder,
    const grpc_channel_args* args) {
  grpc_core::ExecCtx exec_ctx;
  GRPC_API_TRACE("grpc_channel_create_from_binder(target=%p, args=%p)", 2,
                 ((void*)1234, args));

  grpc_transport* transport =
      grpc_create_binder_transport(std::move(endpoint_binder));
  GPR_ASSERT(transport);

  // TODO(b/192207753): check binder alive and ping binder

  // TODO(b/192207758): Figure out if we are required to set authority here
  grpc_arg default_authority_arg = grpc_channel_arg_string_create(
      const_cast<char*>(GRPC_ARG_DEFAULT_AUTHORITY),
      const_cast<char*>("test.authority"));
  grpc_channel_args* final_args =
      grpc_channel_args_copy_and_add(args, &default_authority_arg, 1);
  grpc_error_handle error = GRPC_ERROR_NONE;
  grpc_channel* channel = grpc_channel_create(
      "binder_target_place_holder", final_args, GRPC_CLIENT_DIRECT_CHANNEL,
      transport, nullptr, &error);
  // TODO(mingcl): Handle error properly
  GPR_ASSERT(error == GRPC_ERROR_NONE);
  return channel;
}

}  // namespace internal
}  // namespace grpc
