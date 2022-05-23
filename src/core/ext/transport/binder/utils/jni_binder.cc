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

#include <grpc/support/port_platform.h>

#include "src/core/ext/transport/binder/utils/ndk_binder.h"

#ifndef GRPC_NO_BINDER

#ifdef GPR_SUPPORT_BINDER_TRANSPORT

#include <map>

#include <grpc/support/log.h>

#include "src/core/ext/transport/binder/client/jni_utils.h"
#include "src/core/lib/gpr/tls.h"
#include "src/core/lib/gprpp/ref_counted.h"
#include "src/core/lib/gprpp/sync.h"

namespace {

class JObjectGlobalRef : public grpc_core::RefCounted<JObjectGlobalRef> {
 public:
  JObjectGlobalRef(JNIEnv* env, jobject local_ref) {
    global_ref_ = env->NewGlobalRef(local_ref);
    gpr_log(GPR_ERROR, "New global ref: %p", global_ref_);
    GPR_ASSERT(global_ref_ != nullptr);
  }

  ~JObjectGlobalRef() {
    gpr_log(GPR_ERROR, "Deleting global ref: %p", global_ref_);
    grpc_binder::ndk_util::GetJNIEnv()->DeleteGlobalRef(global_ref_);
  }

  jobject get() const { return global_ref_; }

  // Ref() and UnRef() are called by JNI_AIBinder_incStrong and
  // JNI_AIBinder_decStrong
  void Ref() { ref_count_.RefNonZero(); }

  // Decrements the ref-count and returns true if the ref-count reaches 0.
  bool UnRef() { return ref_count_.Unref(); }

 private:
  jobject global_ref_;
  grpc_core::RefCount ref_count_;
};

JObjectGlobalRef* ToGlobalRef(JNIEnv* env, jobject local_ref) {
  auto global_ref = new JObjectGlobalRef(env, local_ref);
  return global_ref;
}

grpc_binder::ndk_util::AParcel* CreateAParcel(JNIEnv* env, jobject parcel) {
  auto global_ref = new JObjectGlobalRef(env, parcel);
  return reinterpret_cast<grpc_binder::ndk_util::AParcel*>(global_ref);
}

/*
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
};*/

}  // namespace

namespace grpc_binder {
namespace ndk_util {

// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wunused-parameter"

// Types are reinterpreted:
// AIBinder ->  An ID of the binder object in Java. Use together with
// `g_binder_n2j` and `g_binder_j2n` to obtain JObjectGlobalRef
// AParcel -> jobject of android.os.Parcel (global ref) TODO: Probably use
// JObjectGlobalRef* instead?
//
// AIBinder_Class -> custom class defined here
//
// TODO methods should be thread safe?
// TODO how to test these?

void JNI_AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* /*clazz*/) {
}

// maps binder id to userdata
std::map<intptr_t, void*> g_user_data;
// TODO use static initializer
grpc_core::Mutex g_user_data_mu;

// Note that using reference address as keys won't work because of
// https://developer.android.com/training/articles/perf-jni#:~:text=One%20consequence%20of%20this%20is,bit%20value%20on%20consecutive%20calls.
// Here we generate our own binder id
std::atomic<intptr_t> g_next_binder_id{1};
std::map<int, const AIBinder_Class*> g_binder_to_class;

grpc_core::Mutex g_binder_mu;
std::map<intptr_t, JObjectGlobalRef*> g_binder_n2j;
std::map<JObjectGlobalRef*, intptr_t> g_binder_j2n;

void Insert(int id, JObjectGlobalRef* binder_ref) {
  grpc_core::MutexLock lock(&g_binder_mu);
  g_binder_n2j[id] = binder_ref;
  g_binder_j2n[binder_ref] = id;
}

JObjectGlobalRef* ToBinderRef(AIBinder* aibinder) {
  grpc_core::MutexLock lock(&g_binder_mu);
  auto it = g_binder_n2j.find(reinterpret_cast<intptr_t>(aibinder));
  GPR_ASSERT(it != g_binder_n2j.end());
  return it->second;
}

jobject ToJniType(const AParcel* parcel) {
  return reinterpret_cast<const JObjectGlobalRef*>(parcel)->get();
}

void DeleteAIBinder(AIBinder* aibinder) {
  grpc_core::MutexLock lock(&g_binder_mu);
  auto it = g_binder_n2j.find(reinterpret_cast<intptr_t>(aibinder));
  GPR_ASSERT(it != g_binder_n2j.end());
  auto it_j2n = g_binder_j2n.find(it->second);
  GPR_ASSERT(it_j2n != g_binder_j2n.end());
  delete it->second;
  g_binder_n2j.erase(it);
  g_binder_j2n.erase(it_j2n);
}

void* JNI_AIBinder_getUserData(AIBinder* binder) {
  grpc_core::MutexLock lock(&g_user_data_mu);
  GPR_ASSERT(g_user_data.count(reinterpret_cast<intptr_t>(binder)));
  return g_user_data[reinterpret_cast<intptr_t>(binder)];
}

static GPR_THREAD_LOCAL(int) g_calling_uid(-1);

uid_t JNI_AIBinder_getCallingUid() {
  GPR_ASSERT(g_calling_uid != -1);
  return g_calling_uid;
}

AIBinder* JNI_AIBinder_fromJavaBinder(JNIEnv* env, jobject binder) {
  // ndkbinder's AIBinder_fromJavaBinder's implementation probably implies a
  // conversion from localref to globalref+ref counting
  // TODO: probably no need to convert?
  // return FromJniType(ToGlobalRef(env, binder));
  intptr_t binder_id = g_next_binder_id++;
  // convert to global ref
  // TODO: when to delete the ref?
  auto* binder_ref = ToGlobalRef(env, binder);
  Insert(binder_id, binder_ref);
  {
    // Since this binder is from Java, we simply sets its user data to nullptr.
    grpc_core::MutexLock lock(&g_user_data_mu);
    g_user_data[binder_id] = nullptr;
  }
  return reinterpret_cast<AIBinder*>(binder_id);
}

struct class_table {
  AIBinder_Class_onCreate onCreate;
  AIBinder_Class_onDestroy onDestroy;
  AIBinder_Class_onTransact onTransact;
};

std::map<const AIBinder_Class*, class_table> g_class_map;
int g_next_class_key = 1;

class_table BinderIdToClassTable(intptr_t binder_id) {
  grpc_core::MutexLock lock(&g_user_data_mu);
  GPR_ASSERT(g_binder_to_class.count(binder_id));
  const AIBinder_Class* cls = g_binder_to_class[binder_id];
  GPR_ASSERT(g_class_map.count(cls));
  return g_class_map[cls];
}

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
  intptr_t binder_id = g_next_binder_id++;
  // call JNI to allocate a binder object in Java
  jobject local_ref = AIBinderNew(env, binder_id);
  auto* binder_ref = ToGlobalRef(env, local_ref);
  gpr_log(GPR_ERROR, "JNI_AIBinder_new: binder = %p / %p", local_ref,
          binder_ref);
  {
    grpc_core::MutexLock lock(&g_user_data_mu);
    g_user_data[binder_id] = user_data;
    g_binder_to_class[binder_id] = clazz;
  }
  Insert(binder_id, binder_ref);
  return reinterpret_cast<AIBinder*>(binder_id);
}

bool JNI_AIBinder_associateClass(AIBinder* binder, const AIBinder_Class* clazz) {
  // We only call this method for binder passed from Java and always associates
  // no-op class.
  intptr_t binder_id = reinterpret_cast<intptr_t>(binder);
  {
    grpc_core::MutexLock lock(&g_user_data_mu);
    g_binder_to_class[binder_id] = clazz;
  }
  return true;
}

void JNI_AIBinder_incStrong(AIBinder* aibinder) {
  // Per my understanding this is only called if a SpAIBinder is created for the
  // AIBinder?
  // GPR_ASSERT(false);
  // Will implement it anyway
  ToBinderRef(aibinder)->Ref();
}

void JNI_AIBinder_decStrong(AIBinder* aibinder) {
  auto jobject_ref = ToBinderRef(aibinder);
  if (jobject_ref->UnRef()) {
    BinderIdToClassTable(reinterpret_cast<intptr_t>(aibinder))
        .onDestroy(JNI_AIBinder_getUserData(aibinder));
    DeleteAIBinder(aibinder);
  }
}

jobject GetJavaBinder(AIBinder* binder) {
  jobject j_binder;
  GPR_ASSERT(reinterpret_cast<int64_t>(binder) <
             static_cast<int64_t>(g_next_binder_id.load()));
  j_binder = ToBinderRef(binder)->get();
  return j_binder;
}

binder_status_t JNI_AIBinder_transact(AIBinder* binder, transaction_code_t code,
                                      AParcel** in, AParcel** out,
                                      binder_flags_t flags) {
  GPR_ASSERT(flags == FLAG_ONEWAY);
  JNIEnv* env = GetJNIEnv();
  binder_status_t status;
  if (AIBinderTransact(env, GetJavaBinder(binder), code, ToJniType(*in),
                       flags)) {
    status = STATUS_OK;
  } else {
    status = STATUS_UNKNOWN_ERROR;
  }
  // Need to set out because AParcel_delete will be call on `out` later.
  *out = nullptr;
  // According to doc the ownership of the parcel is transferred here. Delete
  // it.
  delete reinterpret_cast<JObjectGlobalRef*>(*in);
  return status;
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

void JNI_AParcel_delete(AParcel* parcel) {
  gpr_log(GPR_ERROR, "AParcel_delete: parcel = %p", parcel);
  if (parcel != nullptr) {
    delete reinterpret_cast<JObjectGlobalRef*>(parcel);
  }
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
  AParcelWriteStrongBinder(env, ToJniType(parcel), GetJavaBinder(binder));
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
  JObjectGlobalRef* java_binder =
      ToGlobalRef(env, AParcelReadStrongBinder(env, ToJniType(parcel)));
  intptr_t binder_id = g_next_binder_id++;
  {
    grpc_core::MutexLock lock(&g_binder_mu);
    g_binder_n2j[binder_id] = java_binder;
    g_binder_j2n[java_binder] = binder_id;
  }
  *binder = reinterpret_cast<AIBinder*>(binder_id);
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
  *in = reinterpret_cast<AParcel*>(
      ToGlobalRef(env, AIBinderPrepareTransaction(env, GetJavaBinder(binder))));
  return STATUS_OK;
}

jobject JNI_AIBinder_toJavaBinder(JNIEnv*, AIBinder* binder) {
  // TODO: probably no need to convert?
  // return ToJniType(binder);
  return ToBinderRef(binder)->get();
}

extern "C" JNIEXPORT void JNICALL
Java_io_grpc_binder_cpp_NativeBinderImpl_onTransaction__IILandroid_os_Parcel_2I(
    JNIEnv* env, jobject /*this*/, jint binder_id, jint tx_code, jobject parcel,
    jint calling_uid) {
  gpr_log(GPR_ERROR,
          "onTransaction: binder_id = %d, tx_code = %d, calling_uid = %d",
          binder_id, tx_code, calling_uid);
  g_calling_uid = calling_uid;
  BinderIdToClassTable(binder_id).onTransact(
      reinterpret_cast<AIBinder*>(binder_id), tx_code,
      CreateAParcel(env, parcel), nullptr);
}

}  // namespace ndk_util
}  // namespace grpc_binder

// #pragma clang diagnostic pop

#endif
#endif
