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

#include <grpc/impl/codegen/port_platform.h>

#include "src/core/ext/transport/binder/client/channel_create_impl.h"

#include <memory>
#include <utility>

#include "src/core/ext/filters/client_channel/client_channel.h"
#include "src/core/ext/filters/client_channel/client_channel_factory.h"
#include "src/core/ext/filters/client_channel/connector.h"
#include "src/core/ext/filters/client_channel/subchannel.h"
#include "src/core/ext/transport/binder/client/jni_utils.h"
#include "src/core/ext/transport/binder/transport/binder_transport.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/binder_android.h"
#include "src/core/lib/channel/channel_args.h"
#include "src/core/lib/surface/api_trace.h"
#include "src/core/lib/surface/channel.h"

std::unique_ptr<grpc_binder::Binder> g_endpoint_binder = nullptr;

#ifdef GPR_SUPPORT_BINDER_TRANSPORT

#include <jni.h>

extern "C" {
// TODO(mingcl): Support multiple endpoint binders
void Java_io_grpc_binder_cpp_SyncServiceConnection_NotifyConnected(
    JNIEnv* jni_env, jobject binder_object) {
  jobject object_pull = grpc_binder::CallStaticJavaMethodForObject(
      jni_env, "io/grpc/binder/cpp/NativeConnectionHelper", "getServiceBinder",
      "()Landroid/os/IBinder;");
  GPR_ASSERT(binder_object != nullptr);
  GPR_ASSERT(object_pull != nullptr);
  ndk::SpAIBinder aibinder =
      grpc_binder::FromJavaBinder(jni_env, binder_object);
  ndk::SpAIBinder aibinder_pull =
      grpc_binder::FromJavaBinder(jni_env, object_pull);
  gpr_log(GPR_ERROR, "binder_object = %p, object_pull = %p", binder_object,
          object_pull);
  gpr_log(GPR_ERROR, "aibinder = %p, aibinder_pull = %p", aibinder.get(),
          aibinder_pull.get());
  auto b = absl::make_unique<grpc_binder::BinderAndroid>(aibinder_pull);
  GPR_ASSERT(b != nullptr);
  g_endpoint_binder = std::move(b);
  // notify binder connector instance somehow
  gpr_log(GPR_ERROR, "%s called", __func__);
}
}

#endif

namespace grpc {

namespace {

class BinderConnector : public grpc_core::SubchannelConnector {
 public:
  BinderConnector() {}
  ~BinderConnector() override {}
  // Probably need to move the setup transport code here so that we don't block
  // the thread
  void Connect(const Args& args, Result* result,
               grpc_closure* notify) override {
    gpr_log(GPR_ERROR, "BinderConnector connect called");

    GPR_ASSERT(g_endpoint_binder != nullptr);
    gpr_log(GPR_ERROR, "args.address = %s", args.address->addr);
    grpc_transport* transport =
        grpc_create_binder_transport_client(std::move(g_endpoint_binder));
    GPR_ASSERT(transport);
    result->channel_args = grpc_channel_args_copy(args.channel_args);
    gpr_log(GPR_ERROR, "args.channel_args = %p", args.channel_args);
    gpr_log(GPR_ERROR, "args.channel_args->args = %p", args.channel_args->args);
    gpr_log(GPR_ERROR, "args.channel_args->num_args = %zu",
            args.channel_args->num_args);
    gpr_log(GPR_ERROR, "result->channel_args = %p", result->channel_args);
    result->transport = transport;

    grpc_core::ExecCtx::Run(DEBUG_LOCATION, notify, GRPC_ERROR_NONE);
  }
  void Shutdown(grpc_error_handle error) override { (void)error; }
};

// probably will need require user to pass JVM* here
class BinderClientChannelFactory : public grpc_core::ClientChannelFactory {
 public:
  grpc_core::RefCountedPtr<grpc_core::Subchannel> CreateSubchannel(
      const grpc_resolved_address& address, const grpc_channel_args* args) override {
    gpr_log(GPR_ERROR, "BinderClientChannelFactory::CreateSubchannel called");
    grpc_arg default_authority_arg = grpc_channel_arg_string_create(
        const_cast<char*>(GRPC_ARG_DEFAULT_AUTHORITY),
        const_cast<char*>("binder.authority"));
    grpc_channel_args* new_args =
        grpc_channel_args_copy_and_add(args, &default_authority_arg, 1);

    gpr_log(GPR_ERROR, "new_args = %p", new_args);
    grpc_core::RefCountedPtr<grpc_core::Subchannel> s =
        grpc_core::Subchannel::Create(
            grpc_core::MakeOrphanable<BinderConnector>(), address, new_args);

    return s;
  }
};

BinderClientChannelFactory* g_factory;
gpr_once g_factory_once = GPR_ONCE_INIT;

void FactoryInit() { g_factory = new BinderClientChannelFactory(); }

}  // namespace

namespace internal {

grpc_channel* CreateDirectBinderChannelImpl(
    std::unique_ptr<grpc_binder::Binder> endpoint_binder,
    const grpc_channel_args* args) {
  grpc_core::ExecCtx exec_ctx;
  GRPC_API_TRACE("grpc_channel_create_from_binder(target=%p, args=%p)", 2,
                 ((void*)1234, args));

  grpc_transport* transport =
      grpc_create_binder_transport_client(std::move(endpoint_binder));
  GPR_ASSERT(transport);

  // TODO(b/192207753): check binder alive and ping binder

  grpc_arg default_authority_arg = grpc_channel_arg_string_create(
      const_cast<char*>(GRPC_ARG_DEFAULT_AUTHORITY),
      const_cast<char*>("binder.authority"));
  grpc_channel_args* final_args =
      grpc_channel_args_copy_and_add(args, &default_authority_arg, 1);
  grpc_error_handle error = GRPC_ERROR_NONE;
  grpc_channel* channel = grpc_channel_create(
      "binder://binder_target_placeholder", final_args,
      GRPC_CLIENT_DIRECT_CHANNEL, transport, nullptr, 0, &error);
  // TODO(mingcl): Handle error properly
  GPR_ASSERT(error == GRPC_ERROR_NONE);
  grpc_channel_args_destroy(final_args);
  return channel;
}

// probably will need require user to pass JVM* here
grpc_channel* CreateClientBinderChannelImpl(const grpc_channel_args* args) {
  grpc_core::ExecCtx exec_ctx;

  gpr_once_init(&g_factory_once, FactoryInit);

  grpc_arg arg = grpc_core::ClientChannelFactory::CreateChannelArg(g_factory);
  const char* arg_to_remove = arg.key;
  grpc_channel_args* new_args = grpc_channel_args_copy_and_add_and_remove(
      args, &arg_to_remove, 1, &arg, 1);
  gpr_log(GPR_ERROR, "new_args = %p", new_args);

  grpc_error_handle error = GRPC_ERROR_NONE;

  grpc_channel_args* new_new_args;
  {
    char* uri = new char[100];
    strcpy(uri, "binder://placeholderholder");
    grpc_arg arg = grpc_channel_arg_string_create(
        const_cast<char*>(GRPC_ARG_SERVER_URI), uri);
    const char* to_remove[] = {GRPC_ARG_SERVER_URI};
    new_new_args = grpc_channel_args_copy_and_add_and_remove(
        new_args, to_remove, 1, &arg, 1);
  }
  gpr_log(GPR_ERROR, "new_new_args = %p", new_new_args);

  grpc_channel* channel = grpc_channel_create(
      "binder://binder_channel_target_placeholder", new_new_args,
      GRPC_CLIENT_CHANNEL, nullptr, nullptr, 0, &error);

  // Clean up.
  grpc_channel_args_destroy(new_new_args);
  grpc_channel_args_destroy(new_args);
  if (channel == nullptr) {
    intptr_t integer;
    grpc_status_code status = GRPC_STATUS_INTERNAL;
    if (grpc_error_get_int(error, GRPC_ERROR_INT_GRPC_STATUS, &integer)) {
      status = static_cast<grpc_status_code>(integer);
    }
    GRPC_ERROR_UNREF(error);
    channel = grpc_lame_client_channel_create(
        "binder://binder_channel_target_placeholder", status,
        "Failed to create binder channel");
  }

  return channel;
}

}  // namespace internal
}  // namespace grpc
