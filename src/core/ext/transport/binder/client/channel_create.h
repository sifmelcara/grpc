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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H

#include <grpc/impl/codegen/port_platform.h>

#ifdef GPR_SUPPORT_BINDER_TRANSPORT

#include <jni.h>

#include "absl/strings/string_view.h"

#include <grpc/impl/codegen/grpc_types.h>
#include <grpcpp/channel.h>
#include <grpcpp/support/channel_arguments.h>

namespace grpc {
namespace experimental {

// Create a new Channel from server package name and service class name
std::shared_ptr<grpc::Channel> CreateBinderChannel(
    void* jni_env_void, jobject application, absl::string_view package_name,
    absl::string_view class_name);

// Create a new Channel from server package name and service class name and with
// custom channel arguments.
std::shared_ptr<grpc::Channel> CreateCustomBinderChannel(
    void* jni_env_void, jobject application, absl::string_view package_name,
    absl::string_view class_name, const ChannelArguments& args);

}  // namespace experimental
}  // namespace grpc

#endif

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_CHANNEL_CREATE_H
