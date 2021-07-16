#include "src/core/ext/transport/binder/client/channel_create.h"

#ifdef ANDROID

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <grpc/grpc.h>
#include <grpc/grpc_posix.h>
#include <grpc/support/log.h>
#include <grpc/support/port_platform.h>
#include <grpcpp/impl/grpc_library.h>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "src/core/ext/transport/binder/client/channel_create_impl.h"
#include "src/core/ext/transport/binder/client/jni_utils.h"
#include "src/core/ext/transport/binder/transport/binder_transport.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/binder_android.h"
#include "src/core/lib/surface/channel.h"
#include "src/core/lib/transport/transport.h"
#include "src/cpp/client/create_channel_internal.h"

namespace grpc {

std::shared_ptr<grpc::Channel> CreateBinderChannel(
    void* jni_env_void, jobject application, absl::string_view package_name,
    absl::string_view class_name) {
  JNIEnv* jni_env = (JNIEnv*)jni_env_void;

  // Turn off clang-format to avoid breaking package name (for substitution)
  // clang-format off
  callStaticJavaMethod(jni_env,
                       "io/grpc/binder/cpp/NativeConnectionHelper",
                       "tryEstablishConnection",
                       "(Landroid/content/Context;)V",
                       application);
  // clang-format on

  gpr_log(GPR_ERROR, "called");
  gpr_log(GPR_ERROR, "sleeping");
  absl::SleepFor(absl::Seconds(3));
  gpr_log(GPR_ERROR, "waked up");

  // clang-format off
  jobject object = callStaticJavaMethodForObject(
      jni_env,
      "io/grpc/binder/cpp/NativeConnectionHelper",
      "getServiceBinder",
      "()Landroid/os/IBinder;");
  // clang-format on

  grpc::internal::GrpcLibrary init_lib;
  init_lib.init();

  return CreateChannelInternal(
      "",
      internal::CreateChannelFromBinderImpl(
          std::make_unique<binder_transport::BinderAndroid>(
              binder_transport::FromJavaBinder(jni_env, object)),
          nullptr),
      std::vector<
          std::unique_ptr<experimental::ClientInterceptorFactoryInterface>>());
}

}  // namespace grpc

#endif  // ANDROID
