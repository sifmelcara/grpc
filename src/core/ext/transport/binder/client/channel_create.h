#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H_

#ifdef ANDROID

#include <grpc/impl/codegen/grpc_types.h>
#include <grpc/support/port_platform.h>
#include <grpcpp/channel.h>
#include <jni.h>

#include "absl/strings/string_view.h"
#include "src/core/ext/transport/binder/wire_format/wire_writer.h"

namespace grpc {

/// Create a new Channel from server package name and service class name
std::shared_ptr<grpc::Channel> CreateBinderChannel(
    void* jni_env_void, jobject application, absl::string_view package_name,
    absl::string_view class_name);

}  // namespace grpc

#endif

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H_
