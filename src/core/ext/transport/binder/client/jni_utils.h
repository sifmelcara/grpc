#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H_

#ifdef ANDROID

#include <jni.h>

#include <string>

// TODO(mingcl): Put these functions in a proper namespace
// TODO(mingcl): Use string_view
void callStaticJavaMethod(JNIEnv* env, const std::string& clazz,
                          const std::string& method, const std::string& type,
                          jobject application);

jobject callStaticJavaMethodForObject(JNIEnv* env, const std::string& clazz,
                                      const std::string& method,
                                      const std::string& type);

#endif

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_CLIENT_JNI_UTILS_H_
