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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H

#if defined(ANDROID) || defined(__ANDROID__)

#include <grpc/support/port_platform.h>

#include <jni.h>

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace grpc_binder {

// Finds NativeConnectionHelper Java class and caches it. This is useful because
// FindClass only works when there is a Java class in the call stack. Typically
// user might want to call this once in a place that is called from Java (ex.
// JNI_OnLoad) so subsequent BinderTransport code can find Java class
jclass FindNativeConnectionHelper(JNIEnv* env);

jclass FindNativeConnectionHelper(
    JNIEnv* env, std::function<void*(std::string)> class_finder);

jclass FindNativeBinderImplHelper(JNIEnv* env);

jclass FindNativeBinderImplHelper(
    JNIEnv* env, std::function<void*(std::string)> class_finder);

// Calls Java method NativeConnectionHelper.tryEstablishConnection
void TryEstablishConnection(JNIEnv* env, jobject application,
                            absl::string_view pkg, absl::string_view cls,
                            absl::string_view action_name,
                            absl::string_view conn_id);

void TryEstablishConnectionWithUri(JNIEnv* env, jobject application,
                                   absl::string_view uri,
                                   absl::string_view conn_id);

// Calls Java method NativeConnectionHelper.isSignatureMatch.
// Will also return false if failed to invoke Java.
bool IsSignatureMatch(JNIEnv* env, jobject context, int uid1, int uid2);

jobject AIBinderNew(JNIEnv* env, int id);
jobject AIBinderPrepareTransaction(JNIEnv* env, jobject binder);

// Returns true if success
bool AIBinderTransact(JNIEnv* env, jobject binder, int code, jobject parcel,
                      int flags);

int AParcelGetDataSize(JNIEnv* env, jobject parcel);

void AParcelWriteInt32(JNIEnv* env, jobject parcel, int32_t value);
void AParcelWriteInt64(JNIEnv* env, jobject parcel, int64_t value);
void AParcelWriteStrongBinder(JNIEnv* env, jobject parcel, jobject binder);
void AParcelWriteString(JNIEnv* env, jobject parcel, const char* string,
                        size_t length);

int32_t AParcelReadInt32(JNIEnv* env, jobject parcel);
int64_t AParcelReadInt64(JNIEnv* env, jobject parcel);
jobject AParcelReadStrongBinder(JNIEnv* env, jobject parcel);
std::string AParcelReadString(JNIEnv* env, jobject parcel);

void AParcelWriteByteArray(JNIEnv* env, jobject parcel, const int8_t* arrayData, size_t length);
std::vector<int8_t> AParcelReadByteArray(JNIEnv* env, jobject parcel);

}  // namespace grpc_binder

#endif

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H
