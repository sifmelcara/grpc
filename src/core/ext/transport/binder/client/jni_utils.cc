#include "src/core/ext/transport/binder/client/jni_utils.h"

#include <grpc/support/log.h>

#ifdef ANDROID

void callStaticJavaMethod(JNIEnv* env, const std::string& clazz,
                          const std::string& method, const std::string& type,
                          jobject application) {
  jclass cl = env->FindClass(clazz.c_str());
  if (cl == nullptr) {
    gpr_log(GPR_ERROR, "No class %s", clazz.c_str());
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", method.c_str());
  }

  env->CallStaticVoidMethod(cl, mid, application);
}

jobject callStaticJavaMethodForObject(JNIEnv* env, const std::string& clazz,
                                      const std::string& method,
                                      const std::string& type) {
  jclass cl = env->FindClass(clazz.c_str());
  if (cl == nullptr) {
    gpr_log(GPR_ERROR, "No class %s", clazz.c_str());
    return nullptr;
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    gpr_log(GPR_ERROR, "No method id %s", method.c_str());
    return nullptr;
  }

  jobject object = env->CallStaticObjectMethod(cl, mid);
  if (object == nullptr) {
    gpr_log(GPR_ERROR, "Got null object from Java");
    return nullptr;
  }

  return object;
}

#endif
