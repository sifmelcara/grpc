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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_CHECK_BINDER_API_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_CHECK_BINDER_API_H

#include <grpc/impl/codegen/port_platform.h>

// This header defines GRPC_SUPPORT_BINDER_TRANSPORT when the necessary
// NDK binder API for binder transport is available.

#if GPR_ANDROID

// For now, we only support Android API level >= 29. We will also need to check
// NDK version with __NDK_MAJOR__ and __NDK_MINOR__ in the future
#if __ANDROID_API__ >= 29
#define GRPC_SUPPORT_BINDER_TRANSPORT 1
#else
#define GRPC_SUPPORT_BINDER_TRANSPORT 0
#endif

#else  //!GPR_ANDROID

#define GRPC_SUPPORT_BINDER_TRANSPORT 0

#endif // GPR_ANDROID

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_CHECK_BINDER_API_H
