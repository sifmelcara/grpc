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

#include <android/log.h>
#include <jni.h>
#include <grpcpp/grpcpp.h>
#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>

#include "src/core/ext/transport/binder/server/binder_server.h"
#include "src/core/ext/transport/binder/server/binder_server_credentials.h"
#include "test/core/transport/binder/end2end/echo_service.h"

std::unique_ptr<grpc::Server> server;

extern "C" JNIEXPORT void JNICALL
Java_io_grpc_binder_cpp_exampleserver_ExportedEndpointService_init_1grpc_1server(
    JNIEnv* env, jobject /*this*/) {
  __android_log_print(ANDROID_LOG_INFO, "DemoServer", "Line number %d", __LINE__);
  if (server != nullptr) {
    // Already initiated
    return;
  }
  grpc_endpoint_binder_pool_init();
  static grpc_binder::end2end_testing::EchoServer service;
  grpc::ServerBuilder server_builder;
  server_builder.RegisterService(&service);
  server_builder.AddListeningPort("binder://example.service", grpc::experimental::BinderServerCredentials());
  server = server_builder.BuildAndStart();
}

extern "C" JNIEXPORT jobject JNICALL
Java_io_grpc_binder_cpp_exampleserver_ExportedEndpointService_get_1endpoint_1binder(
    JNIEnv* env, jobject /*this*/) {
  __android_log_print(ANDROID_LOG_INFO, "DemoServer", "Line number %d", __LINE__);
  auto ai_binder = static_cast<AIBinder*>(grpc::experimental::binder::GetEndpointBinder("example.service"));
  __android_log_print(ANDROID_LOG_INFO, "DemoServer", "ai_binder = %p", ai_binder);
  return AIBinder_toJavaBinder(env, ai_binder);
}
