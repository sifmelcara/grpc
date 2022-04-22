
#include <grpc/support/port_platform.h>

#include "src/core/ext/transport/binder/utils/ndk_binder.h"

#ifndef GRPC_NO_BINDER

#ifdef GPR_SUPPORT_BINDER_TRANSPORT

#include <map>

#include <grpc/support/log.h>

#include "src/core/ext/transport/binder/client/jni_utils.h"
#include "src/core/lib/gprpp/sync.h"

namespace {
// TODO some kind of ref counting
jobject ToGlobalRef(JNIEnv* env, jobject local_ref) {
  jobject global_ref = env->NewGlobalRef(local_ref);
  env->DeleteLocalRef(local_ref);
  GPR_ASSERT(global_ref != nullptr);
  gpr_log(GPR_ERROR, "New global ref: %p", global_ref);
  return global_ref;
}

// AIBinder* -> jobject
// AParcel* -> jobject

jobject ToJniType(const grpc_binder::ndk_util::AIBinder* b) {
  return reinterpret_cast<jobject>(
      const_cast<grpc_binder::ndk_util::AIBinder*>(b));
}

jobject ToJniType(const grpc_binder::ndk_util::AParcel* b) {
  return reinterpret_cast<jobject>(
      const_cast<grpc_binder::ndk_util::AParcel*>(b));
}

class FromJniType {
 public:
  FromJniType(jobject ptr) : ptr_(ptr) {}
  template <typename T>
  operator T*() const {
    constexpr bool convertible =
        std::is_same<T, grpc_binder::ndk_util::AIBinder>::value ||
        std::is_same<T, const grpc_binder::ndk_util::AParcel>::value ||
        std::is_same<T, grpc_binder::ndk_util::AParcel>::value;
    static_assert(convertible, "Cannot convert jobject");
    return reinterpret_cast<T*>(ptr_);
  }

 private:
  jobject ptr_;
};

}  // namespace

namespace grpc_binder {
namespace ndk_util {

// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wunused-parameter"

// Types are reinterpreted:
// AIBinder -> jobject of android.os.Binder (global ref)
// AIParcel -> jobject of android.os.Parcel (global ref)
// AIBinder_Class -> custom class defined here
//
// TODO methods should be thread safe?
// TODO how to test these?

void JNI_AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* /*clazz*/) {
}

// maps binder to userdata
std::map<void*, void*> g_user_data;
// TODO use static initializer
grpc_core::Mutex g_user_data_mu;

std::map<void*, const AIBinder_Class*> g_binder_to_class;

void* JNI_AIBinder_getUserData(AIBinder* binder) {
  GPR_ASSERT(g_user_data.count(binder));
  grpc_core::MutexLock lock(&g_user_data_mu);
  return g_user_data[binder];
}

uid_t JNI_AIBinder_getCallingUid() {
  // unimplemented
  return 0;
}

AIBinder* JNI_AIBinder_fromJavaBinder(JNIEnv* env, jobject binder) {
  // ndkbinder's AIBinder_fromJavaBinder's implementation probably implies a
  // conversion from localref to globalref+ref counting
  // TODO: probably no need to convert?
  return FromJniType(ToGlobalRef(env, binder));
}

struct class_table {
  AIBinder_Class_onCreate onCreate;
  AIBinder_Class_onDestroy onDestroy;
  AIBinder_Class_onTransact onTransact;
};

std::map<const AIBinder_Class*, class_table> g_class_map;
int g_next_class_key = 1;

AIBinder_Class* JNI_AIBinder_Class_define(
    const char* /*interfaceDescriptor*/, AIBinder_Class_onCreate onCreate,
    AIBinder_Class_onDestroy onDestroy, AIBinder_Class_onTransact onTransact) {
  g_class_map[reinterpret_cast<AIBinder_Class*>(g_next_class_key)] =
      class_table{onCreate, onDestroy, onTransact};
  return reinterpret_cast<AIBinder_Class*>(g_next_class_key++);
}

AIBinder* JNI_AIBinder_new(const AIBinder_Class* clazz, void* args) {
  GPR_ASSERT(g_class_map.count(clazz) != 0);
  void* user_data = g_class_map[clazz].onCreate(args);
  JNIEnv* env = GetJNIEnv();
  // call JNI to allocate a binder object in Java
  jobject binder = ToGlobalRef(env, AIBinderNew(env));
  {
    grpc_core::MutexLock lock(&g_user_data_mu);
    g_user_data[binder] = user_data;
    g_binder_to_class[binder] = clazz;
  }
  return FromJniType(binder);
}

bool JNI_AIBinder_associateClass(AIBinder*, const AIBinder_Class*) {
  // We only call this method for binder passed from Java and always associates
  // no-op class.
  return true;
}

void JNI_AIBinder_incStrong(AIBinder*) {
  // TODO ref counting
}

void JNI_AIBinder_decStrong(AIBinder*) {
  // TODO ref counting
}

binder_status_t JNI_AIBinder_transact(AIBinder* binder, transaction_code_t code,
                                      AParcel** in, AParcel** /*out*/,
                                      binder_flags_t flags) {
  GPR_ASSERT(flags == FLAG_ONEWAY);
  JNIEnv* env = GetJNIEnv();
  return AIBinderTransact(env, ToJniType(binder), code, ToJniType(*in), flags);
  // TODO: delete `in`. According to doc the ownership of the parcel is
  // transferred here (the ownership doesn't transfer all the way to Java code
  // right?)
}

binder_status_t JNI_AParcel_readByteArray(
    const AParcel* parcel, void* arrayData,
    AParcel_byteArrayAllocator allocator) {
  JNIEnv* env = GetJNIEnv();
  // TODO: avoid unnecessary copy here
  std::vector<int8_t> data = AParcelReadByteArray(env, ToJniType(parcel));
  int8_t* buffer;
  // The required buffer size includes null terminator.
  allocator(arrayData, data.size(), &buffer);
  memcpy(buffer, data.data(), data.size());
  return STATUS_OK;
}

void JNI_AParcel_delete(AParcel* /*parcel*/) {
  // TODO: DeleteGlobalRef?
}

int32_t JNI_AParcel_getDataSize(const AParcel* parcel) {
  JNIEnv* env = GetJNIEnv();
  return AParcelGetDataSize(env, ToJniType(parcel));
}

binder_status_t JNI_AParcel_writeInt32(AParcel* parcel, int32_t value) {
  JNIEnv* env = GetJNIEnv();
  AParcelWriteInt32(env, ToJniType(parcel), value);
  return STATUS_OK;
}
binder_status_t JNI_AParcel_writeInt64(AParcel* parcel, int64_t value) {
  JNIEnv* env = GetJNIEnv();
  AParcelWriteInt64(env, ToJniType(parcel), value);
  return STATUS_OK;
}
binder_status_t JNI_AParcel_writeStrongBinder(AParcel* parcel,
                                              AIBinder* binder) {
  JNIEnv* env = GetJNIEnv();
  AParcelWriteStrongBinder(env, ToJniType(parcel), ToJniType(binder));
  return STATUS_OK;
}
binder_status_t JNI_AParcel_writeString(AParcel* parcel, const char* string,
                                        int32_t length) {
  JNIEnv* env = GetJNIEnv();
  AParcelWriteString(env, ToJniType(parcel), string, length);
  return STATUS_OK;
}

binder_status_t JNI_AParcel_readInt32(const AParcel* parcel, int32_t* value) {
  JNIEnv* env = GetJNIEnv();
  *value = AParcelReadInt32(env, ToJniType(parcel));
  return STATUS_OK;
}

binder_status_t JNI_AParcel_readInt64(const AParcel* parcel, int64_t* value) {
  JNIEnv* env = GetJNIEnv();
  *value = AParcelReadInt64(env, ToJniType(parcel));
  return STATUS_OK;
}

binder_status_t JNI_AParcel_readString(const AParcel* parcel, void* stringData,
                                       AParcel_stringAllocator allocator) {
  JNIEnv* env = GetJNIEnv();
  std::string s = AParcelReadString(env, ToJniType(parcel));
  char* buffer;
  // The required buffer size includes null terminator.
  size_t len = s.length() + 1;
  allocator(stringData, len, &buffer);
  strncpy(buffer, s.c_str(), len);
  return STATUS_OK;
}

binder_status_t JNI_AParcel_readStrongBinder(const AParcel* parcel,
                                             AIBinder** binder) {
  JNIEnv* env = GetJNIEnv();
  *binder = FromJniType(
      ToGlobalRef(env, AParcelReadStrongBinder(env, ToJniType(parcel))));
  return STATUS_OK;
}

binder_status_t JNI_AParcel_writeByteArray(AParcel* parcel,
                                           const int8_t* arrayData,
                                           int32_t length) {
  JNIEnv* env = GetJNIEnv();
  AParcelWriteByteArray(env, ToJniType(parcel), arrayData, length);
  return STATUS_OK;
}

binder_status_t JNI_AIBinder_prepareTransaction(AIBinder* binder,
                                                AParcel** in) {
  JNIEnv* env = GetJNIEnv();
  *in = FromJniType(ToGlobalRef(
      env, AIBinderPrepareTransaction(env, reinterpret_cast<jobject>(binder))));
  return STATUS_OK;
}

jobject JNI_AIBinder_toJavaBinder(JNIEnv*, AIBinder* binder) {
  // TODO: probably no need to convert?
  return ToJniType(binder);
}

extern "C" JNIEXPORT void JNICALL
Java_io_grpc_binder_cpp_NativeBinderImpl_onTransaction__Landroid_os_IBinder_2ILandroid_os_Parcel_2(
    JNIEnv*, jobject /*this*/, jobject ibinder, jint tx_code,
    jobject parcel) {
  class_table ct;
  {
    grpc_core::MutexLock lock(&g_user_data_mu);
    ct = g_class_map[g_binder_to_class[ibinder]];
  }
  ct.onTransact(FromJniType(ibinder), tx_code, FromJniType(parcel), nullptr);
}

}  // namespace ndk_util
}  // namespace grpc_binder

// #pragma clang diagnostic pop

#endif
#endif
