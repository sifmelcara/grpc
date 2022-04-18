
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
binder_status_t JNI_AParcel_readByteArray(const AParcel* parcel, void* arrayData,
                                      AParcel_byteArrayAllocator allocator);
void JNI_AParcel_delete(AParcel* parcel);
int32_t JNI_AParcel_getDataSize(const AParcel* parcel);
binder_status_t JNI_AParcel_writeInt32(AParcel* parcel, int32_t value);
binder_status_t JNI_AParcel_writeInt64(AParcel* parcel, int64_t value);
binder_status_t JNI_AParcel_writeStrongBinder(AParcel* parcel, AIBinder* binder);
binder_status_t JNI_AParcel_writeString(AParcel* parcel, const char* string,
                                    int32_t length);
binder_status_t JNI_AParcel_readInt32(const AParcel* parcel, int32_t* value);
binder_status_t JNI_AParcel_readInt64(const AParcel* parcel, int64_t* value);
binder_status_t JNI_AParcel_readString(const AParcel* parcel, void* stringData,
                                   AParcel_stringAllocator allocator);
binder_status_t JNI_AParcel_readStrongBinder(const AParcel* parcel,
                                         AIBinder** binder);
binder_status_t JNI_AParcel_writeByteArray(AParcel* parcel, const int8_t* arrayData,
                                       int32_t length);
binder_status_t JNI_AIBinder_prepareTransaction(AIBinder* binder, AParcel** in);
jobject JNI_AIBinder_toJavaBinder(JNIEnv* env, AIBinder* binder);

}}

#endif /*GPR_SUPPORT_BINDER_TRANSPORT*/

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_JNI_BINDER_H
