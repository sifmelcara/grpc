// Copyright 2022 gRPC authors.
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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_JNI_BINDER_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_JNI_BINDER_H

#include <grpc/support/port_platform.h>

#ifdef GPR_SUPPORT_BINDER_TRANSPORT

#include "src/core/ext/transport/binder/utils/ndk_binder.h"

namespace grpc_binder {
namespace ndk_util {

void JNI_AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* clazz);
void* JNI_AIBinder_getUserData(AIBinder* binder);
uid_t JNI_AIBinder_getCallingUid();
AIBinder* JNI_AIBinder_fromJavaBinder(JNIEnv* env, jobject binder);
AIBinder_Class* JNI_AIBinder_Class_define(const char* interfaceDescriptor,
                                          AIBinder_Class_onCreate onCreate,
                                          AIBinder_Class_onDestroy onDestroy,
                                          AIBinder_Class_onTransact onTransact);
AIBinder* JNI_AIBinder_new(const AIBinder_Class* clazz, void* args);
bool JNI_AIBinder_associateClass(AIBinder* binder, const AIBinder_Class* clazz);
void JNI_AIBinder_incStrong(AIBinder* binder);
void JNI_AIBinder_decStrong(AIBinder* binder);
binder_status_t JNI_AIBinder_transact(AIBinder* binder, transaction_code_t code,
                                      AParcel** in, AParcel** out,
                                      binder_flags_t flags);
binder_status_t JNI_AParcel_readByteArray(const AParcel* parcel,
                                          void* arrayData,
                                          AParcel_byteArrayAllocator allocator);
void JNI_AParcel_delete(AParcel* parcel);
int32_t JNI_AParcel_getDataSize(const AParcel* parcel);
binder_status_t JNI_AParcel_writeInt32(AParcel* parcel, int32_t value);
binder_status_t JNI_AParcel_writeInt64(AParcel* parcel, int64_t value);
binder_status_t JNI_AParcel_writeStrongBinder(AParcel* parcel,
                                              AIBinder* binder);
binder_status_t JNI_AParcel_writeString(AParcel* parcel, const char* string,
                                        int32_t length);
binder_status_t JNI_AParcel_readInt32(const AParcel* parcel, int32_t* value);
binder_status_t JNI_AParcel_readInt64(const AParcel* parcel, int64_t* value);
binder_status_t JNI_AParcel_readString(const AParcel* parcel, void* stringData,
                                       AParcel_stringAllocator allocator);
binder_status_t JNI_AParcel_readStrongBinder(const AParcel* parcel,
                                             AIBinder** binder);
binder_status_t JNI_AParcel_writeByteArray(AParcel* parcel,
                                           const int8_t* arrayData,
                                           int32_t length);
binder_status_t JNI_AIBinder_prepareTransaction(AIBinder* binder, AParcel** in);
jobject JNI_AIBinder_toJavaBinder(JNIEnv* env, AIBinder* binder);

}  // namespace ndk_util
}  // namespace grpc_binder

#endif /*GPR_SUPPORT_BINDER_TRANSPORT*/

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_JNI_BINDER_H
