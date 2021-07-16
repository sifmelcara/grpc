#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_ANDROID_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_ANDROID_H_

#ifdef ANDROID

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <jni.h>

#include <memory>

#include "src/core/ext/transport/binder/wire_format/binder.h"

// TODO(b/192208764): move this check to somewhere else
#if __ANDROID_API__ < 30
// Please follow
// https://g3doc.corp.google.com/third_party/android/ndk/g3doc/api_level.md?cl=head
// to set API level. TODO(mingcl): add build instruction in README.md"
#error "We only support API level >= 30."
#endif

namespace binder_transport {

ndk::SpAIBinder FromJavaBinder(JNIEnv* jni_env, jobject binder);

// TODO(mingcl): Find a better name for these classes

class BinderAndroid;

class InputParcelAndroid final : public InputParcelInterface {
 public:
  InputParcelAndroid() = default;
  explicit InputParcelAndroid(AParcel* parcel) : parcel_(parcel) {}
  ~InputParcelAndroid() override = default;

  binder_status_t GetDataPosition() const override;
  binder_status_t SetDataPosition(int32_t pos) override;
  binder_status_t WriteInt32(int32_t data) override;
  binder_status_t WriteBinder(HasRawBinder* binder) override;
  binder_status_t WriteString(absl::string_view s) override;
  binder_status_t WriteByteArray(const int8_t* buffer, int32_t length) override;

 private:
  AParcel* parcel_ = nullptr;

  friend class BinderAndroid;
};

class OutputParcelAndroid final : public OutputParcelInterface {
 public:
  OutputParcelAndroid() = default;
  // TODO(waynetu): Get rid of the const_cast.
  explicit OutputParcelAndroid(const AParcel* parcel)
      : parcel_(const_cast<AParcel*>(parcel)) {}
  ~OutputParcelAndroid() override = default;

  binder_status_t ReadInt32(int32_t* data) const override;
  binder_status_t ReadBinder(std::unique_ptr<Binder>* data) const override;
  binder_status_t ReadByteArray(std::string* data) const override;
  // FIXME(waynetu): Fix the interface.
  binder_status_t ReadString(char data[111]) const override;

 private:
  AParcel* parcel_ = nullptr;

  friend class BinderAndroid;
};

class BinderAndroid final : public Binder {
 public:
  explicit BinderAndroid(ndk::SpAIBinder binder)
      : binder_(binder),
        input_parcel_(std::make_unique<InputParcelAndroid>()),
        output_parcel_(std::make_unique<OutputParcelAndroid>()) {}
  ~BinderAndroid() override = default;

  void* GetRawBinder() override { return binder_.get(); }

  void Initialize() override;
  binder_status_t PrepareTransaction() override;
  binder_status_t Transact(BinderTransportTxCode tx_code,
                           binder_flags_t flag) override;

  InputParcelInterface* GetInputParcel() const override {
    return input_parcel_.get();
  }
  OutputParcelInterface* GetOutputParcel() const override {
    return output_parcel_.get();
  };

  std::unique_ptr<TransactionReceiver> ConstructTxReceiver(
      TransactionReceiver::OnTransactCb transact_cb) const override;

 private:
  ndk::SpAIBinder binder_;
  std::unique_ptr<InputParcelAndroid> input_parcel_;
  std::unique_ptr<OutputParcelAndroid> output_parcel_;
};

class TransactionReceiverAndroid final : public TransactionReceiver {
 public:
  explicit TransactionReceiverAndroid(OnTransactCb transaction_cb);
  ~TransactionReceiverAndroid() override;
  void* GetRawBinder() override { return binder_; }

 private:
  AIBinder* binder_;
  OnTransactCb transact_cb_;
};

}  // namespace binder_transport

#endif

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_ANDROID_H_
