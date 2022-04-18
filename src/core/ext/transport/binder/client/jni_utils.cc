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

#include <grpc/support/port_platform.h>

#include "src/core/ext/transport/binder/client/jni_utils.h"

#ifndef GRPC_NO_BINDER

#include <grpc/support/log.h>

#if defined(ANDROID) || defined(__ANDROID__)

namespace {
constexpr char kNativeConnectionHelper[] =
    "io/grpc/binder/cpp/NativeConnectionHelper";
constexpr char kNativeBinderImpl[] = "io/grpc/binder/cpp/NativeBinderImpl";
}  // namespace

namespace grpc_binder {

jclass FindNativeConnectionHelper(JNIEnv* env) {
  return FindNativeConnectionHelper(
      env, [env](std::string cl) { return env->FindClass(cl.c_str()); });
}

jclass FindNativeConnectionHelper(
    JNIEnv* env, std::function<void*(std::string)> class_finder) {
  auto do_find = [env, class_finder]() {
    jclass cl = static_cast<jclass>(class_finder(kNativeConnectionHelper));
    if (cl == nullptr) {
      return cl;
    }
    jclass global_cl = static_cast<jclass>(env->NewGlobalRef(cl));
    env->DeleteLocalRef(cl);
    GPR_ASSERT(global_cl != nullptr);
    return global_cl;
  };
  static jclass connection_helper_class = do_find();
  if (connection_helper_class != nullptr) {
    return connection_helper_class;
  }
  // Some possible reasons:
  //   * There is no Java class in the call stack and this is not invoked
  //   from JNI_OnLoad
  //   * The APK does not correctly depends on the helper class, or the
  //   class get shrinked
  gpr_log(GPR_ERROR,
          "Cannot find binder transport Java helper class. Did you invoke "
          "grpc::experimental::InitializeBinderChannelJavaClass correctly "
          "beforehand? Did the APK correctly include the connection helper "
          "class (i.e depends on build target "
          "src/core/ext/transport/binder/java/io/grpc/binder/"
          "cpp:connection_helper) ?");
  // TODO(mingcl): Maybe it is worth to try again so the failure can be fixed
  // by invoking this function again at a different thread.
  return nullptr;
}

jclass FindNativeBinderImplHelper(JNIEnv* env) {
  return FindNativeBinderImplHelper(
      env, [env](std::string cl) { return env->FindClass(cl.c_str()); });
}

jclass FindNativeBinderImplHelper(
    JNIEnv* env, std::function<void*(std::string)> class_finder) {
  auto do_find = [env, class_finder]() {
    jclass cl = static_cast<jclass>(class_finder(kNativeBinderImpl));
    if (cl == nullptr) {
      return cl;
    }
    jclass global_cl = static_cast<jclass>(env->NewGlobalRef(cl));
    env->DeleteLocalRef(cl);
    GPR_ASSERT(global_cl != nullptr);
    return global_cl;
  };
  static jclass binder_impl_class = do_find();
  if (binder_impl_class != nullptr) {
    return binder_impl_class;
  }
  return nullptr;
}

template <typename T>
T CallNativeBinderImplStaticMethod(
    JNIEnv* env, absl::string_view method, absl::string_view type,
    std::function<T(jclass, jmethodID)> jni_func) {
  jclass cl = FindNativeBinderImplHelper(env);
  if (cl == nullptr) {
    gpr_log(GPR_ERROR, "Can't find class");
    // TODO: return optional<T> instead to handle the case when cl is nullptr?
  }
  jmethodID mid = env->GetStaticMethodID(cl, std::string(method).c_str(),
                                         std::string(type).c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", std::string(method).c_str());
  }
  return jni_func(cl, mid);
}

jobject AIBinderNew(JNIEnv* env, int binder_id) {
  return CallNativeBinderImplStaticMethod<jobject>(
      env, "AIBinderNew",
      "(I)Landroid/os/IBinder;",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticObjectMethod(cl, mid, binder_id);
      });
}


jobject AIBinderPrepareTransaction(JNIEnv* env, jobject binder) {
  return CallNativeBinderImplStaticMethod<jobject>(
      env, "AIBinderPrepareTransaction",
      "(Landroid/os/IBinder;)Landroid/os/Parcel;",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticObjectMethod(cl, mid, binder);
      });
}

bool AIBinderTransact(JNIEnv* env, jobject binder, int code, jobject parcel,
                      int flags) {
  return CallNativeBinderImplStaticMethod<bool>(
      env, "AIBinderTransact",
      "(Landroid/os/IBinder;ILandroid/os/Parcel;I)Z",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticBooleanMethod(cl, mid, binder, code, parcel,
                                            flags) == JNI_TRUE;
      });
}

int AParcelGetDataSize(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<int>(
      env, "AParcelGetDataSize", "(Landroid/os/Parcel;)I",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticIntMethod(cl, mid, parcel);
      });
}

void AParcelWriteInt32(JNIEnv* env, jobject parcel, int32_t value) {
  return CallNativeBinderImplStaticMethod<void>(
      env, "AParcelWriteInt32", "(Landroid/os/Parcel;I)V",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticVoidMethod(cl, mid, parcel, value);
      });
}

void AParcelWriteInt64(JNIEnv* env, jobject parcel, int64_t value) {
  return CallNativeBinderImplStaticMethod<void>(
      env, "AParcelWriteInt64", "(Landroid/os/Parcel;J)V",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticVoidMethod(cl, mid, parcel, value);
      });
}

void AParcelWriteStrongBinder(JNIEnv* env, jobject parcel, jobject binder) {
  return CallNativeBinderImplStaticMethod<void>(
      env, "AParcelWriteStrongBinder",
      "(Landroid/os/Parcel;Landroid/os/IBinder;)V",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticVoidMethod(cl, mid, parcel, binder);
      });
}

void AParcelWriteString(JNIEnv* env, jobject parcel, const char* str,
                        size_t length) {
  return CallNativeBinderImplStaticMethod<void>(
      env, "AParcelWriteString", "(Landroid/os/Parcel;Ljava/lang/String;)V",
      [=](jclass cl, jmethodID mid) {
        jstring jstr = env->NewStringUTF(std::string(str, length).c_str());
        env->CallStaticVoidMethod(cl, mid, parcel, jstr);
        env->DeleteLocalRef(jstr);
      });
}

int32_t AParcelReadInt32(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<int32_t>(
      env, "AParcelReadInt32", "(Landroid/os/Parcel;)I",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticIntMethod(cl, mid, parcel);
      });
}
int64_t AParcelReadInt64(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<int64_t>(
      env, "AParcelReadInt64", "(Landroid/os/Parcel;)J",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticLongMethod(cl, mid, parcel);
      });
}
jobject AParcelReadStrongBinder(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<jobject>(
      env, "AParcelReadStrongBinder",
      "(Landroid/os/Parcel;)Landroid/os/IBinder;",
      [=](jclass cl, jmethodID mid) {
        return env->CallStaticObjectMethod(cl, mid, parcel);
      });
}
std::string AParcelReadString(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<std::string>(
      env, "AParcelReadString", "(Landroid/os/Parcel;)Ljava/lang/String;",
      [=](jclass cl, jmethodID mid) {
        jstring jstr = (jstring)env->CallStaticObjectMethod(cl, mid, parcel);
        jboolean isCopy;
        const char* utf = env->GetStringUTFChars(jstr, &isCopy);
        std::string str = utf;
        if (isCopy == JNI_TRUE) {
          env->ReleaseStringUTFChars(jstr, utf);
        }
        return str;
      });
}

void AParcelWriteByteArray(JNIEnv* env, jobject parcel, const int8_t* arrayData,
                           size_t length) {
  return CallNativeBinderImplStaticMethod<void>(
      env, "AParcelWriteByteArray", "(Landroid/os/Parcel;[B)V", [=](jclass cl, jmethodID mid) {
        jbyteArray arr;
        arr = env->NewByteArray(length);
        if (arr == nullptr) {
          // TODO: OOM? Do something.
          return;
        }
        env->SetByteArrayRegion(arr, 0, length, arrayData);
        env->CallStaticVoidMethod(cl, mid, parcel, arr);
      });
}

std::vector<int8_t> AParcelReadByteArray(JNIEnv* env, jobject parcel) {
  return CallNativeBinderImplStaticMethod<std::vector<int8_t>>(
      env, "AParcelReadByteArray", "(Landroid/os/Parcel;)[B", [=](jclass cl, jmethodID mid) {
        jbyteArray jarr =
            (jbyteArray)env->CallStaticObjectMethod(cl, mid, parcel);
        jboolean isCopy;
        jbyte* b = env->GetByteArrayElements(jarr, &isCopy);
        jint len = env->GetArrayLength(jarr);
        std::vector<int8_t> vec(reinterpret_cast<int8_t*>(b),
                                reinterpret_cast<int8_t*>(b + len));
        if (isCopy == JNI_TRUE) {
          env->ReleaseByteArrayElements(jarr, b, 0);
        }
        return vec;
      });
}

void TryEstablishConnection(JNIEnv* env, jobject application,
                            absl::string_view pkg, absl::string_view cls,
                            absl::string_view action_name,
                            absl::string_view conn_id) {
  std::string method = "tryEstablishConnection";
  std::string type =
      "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/"
      "lang/String;Ljava/lang/String;)V";

  jclass cl = FindNativeConnectionHelper(env);
  if (cl == nullptr) {
    return;
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", method.c_str());
  }

  env->CallStaticVoidMethod(cl, mid, application,
                            env->NewStringUTF(std::string(pkg).c_str()),
                            env->NewStringUTF(std::string(cls).c_str()),
                            env->NewStringUTF(std::string(action_name).c_str()),
                            env->NewStringUTF(std::string(conn_id).c_str()));
}

void TryEstablishConnectionWithUri(JNIEnv* env, jobject application,
                                   absl::string_view uri,
                                   absl::string_view conn_id) {
  std::string method = "tryEstablishConnectionWithUri";
  std::string type =
      "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V";

  jclass cl = FindNativeConnectionHelper(env);
  if (cl == nullptr) {
    return;
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", method.c_str());
  }

  env->CallStaticVoidMethod(cl, mid, application,
                            env->NewStringUTF(std::string(uri).c_str()),
                            env->NewStringUTF(std::string(conn_id).c_str()));
}

bool IsSignatureMatch(JNIEnv* env, jobject context, int uid1, int uid2) {
  const std::string method = "isSignatureMatch";
  const std::string type = "(Landroid/content/Context;II)Z";

  jclass cl = FindNativeConnectionHelper(env);
  if (cl == nullptr) {
    return false;
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", method.c_str());
  }

  jboolean result = env->CallStaticBooleanMethod(cl, mid, context, uid1, uid2);
  return result == JNI_TRUE;
}

}  // namespace grpc_binder

#endif
#endif
