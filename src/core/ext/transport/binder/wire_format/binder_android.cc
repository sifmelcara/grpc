#include "src/core/ext/transport/binder/wire_format/binder_android.h"

#ifdef ANDROID

#include <grpc/support/log.h>

#include <map>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"

namespace {

struct AtomicCallback {
  explicit AtomicCallback(void* callback) : mu{}, callback(callback) {}
  absl::Mutex mu;
  void* callback;
};

void* f_onCreate_with_mutex(void* callback) {
  return new AtomicCallback(callback);
}

void* f_onCreate_noop(void* args) { return nullptr; }
void f_onDestroy_noop(void* userData) {}

// TODO(mingcl): Consider if thread safety is a requirement here
binder_status_t f_onTransact(AIBinder* binder, transaction_code_t code,
                             const AParcel* in, AParcel* out) {
  gpr_log(GPR_INFO, __func__);
  gpr_log(GPR_INFO, "tx code = %u", code);
  auto* user_data =
      reinterpret_cast<AtomicCallback*>(AIBinder_getUserData(binder));
  absl::MutexLock(&user_data->mu);

  // TODO(waynetu): What should be returned here?
  if (!user_data->callback) return STATUS_OK;

  auto* callback =
      reinterpret_cast<binder_transport::TransactionReceiver::OnTransactCb*>(
          user_data->callback);
  // Wrap the parcel in a OutputParcelInterface.
  std::unique_ptr<binder_transport::OutputParcelInterface> output =
      std::make_unique<binder_transport::OutputParcelAndroid>(in);
  // The lock should be released "after" the callback finishes.
  return (*callback)(code, output.get());
}
}  // namespace

namespace binder_transport {

ndk::SpAIBinder FromJavaBinder(JNIEnv* jni_env, jobject binder) {
  return ndk::SpAIBinder(AIBinder_fromJavaBinder(jni_env, binder));
}

TransactionReceiverAndroid::TransactionReceiverAndroid(OnTransactCb transact_cb)
    : transact_cb_(transact_cb) {
  // TODO(mingcl): For now interface descriptor is always empty, figure out if
  // we want it to be something more meaningful (we can probably manually change
  // interface descriptor by modifying Java code's reply to
  // os.IBinder.INTERFACE_TRANSACTION)
  AIBinder_Class* aibinder_class = AIBinder_Class_define(
      /*interfaceDescriptor=*/"", f_onCreate_with_mutex, f_onDestroy_noop,
      f_onTransact);

  // Pass the on-transact callback to the on-create function of the binder. The
  // on-create function equips the callback with a mutex and gives it to the
  // user data stored in the binder which can be retrieved later.
  binder_ = AIBinder_new(aibinder_class, &transact_cb_);
  GPR_ASSERT(binder_);
  gpr_log(GPR_INFO, "AIBinder_associateClass = %d",
          static_cast<int>(AIBinder_associateClass(binder_, aibinder_class)));
}

TransactionReceiverAndroid::~TransactionReceiverAndroid() {
  auto* user_data =
      reinterpret_cast<AtomicCallback*>(AIBinder_getUserData(binder_));
  {
    absl::MutexLock(&user_data->mu);
    // Set the callback to null so that future calls to on-trasact are awared
    // that the transaction receiver had been deallocated.
    user_data->callback = nullptr;
  }
  // Release the binder.
  AIBinder_decStrong(binder_);
}

namespace {

binder_status_t f_onTransact_noop(AIBinder* binder, transaction_code_t code,
                                  const AParcel* in, AParcel* out) {
  return {};
}

void AssociateWithNoopClass(AIBinder* binder) {
  // For some reasone we need to associate class before using it..
  AIBinder_Class* aibinder_class = AIBinder_Class_define(
      "", f_onCreate_noop, f_onDestroy_noop, f_onTransact_noop);
  gpr_log(GPR_INFO, "AIBinder_associateClass = %d",
          static_cast<int>(AIBinder_associateClass(binder, aibinder_class)));
}

}  // namespace

void BinderAndroid::Initialize() {
  AIBinder* binder = binder_.get();
  AssociateWithNoopClass(binder);
}

binder_status_t BinderAndroid::PrepareTransaction() {
  AIBinder* binder = binder_.get();
  return AIBinder_prepareTransaction(binder, &input_parcel_->parcel_);
}

binder_status_t BinderAndroid::Transact(BinderTransportTxCode tx_code,
                                        binder_flags_t flag) {
  AIBinder* binder = binder_.get();
  return AIBinder_transact(binder, static_cast<transaction_code_t>(tx_code),
                           &input_parcel_->parcel_, &output_parcel_->parcel_,
                           flag);
}

std::unique_ptr<TransactionReceiver> BinderAndroid::ConstructTxReceiver(
    TransactionReceiver::OnTransactCb transact_cb) const {
  return std::make_unique<TransactionReceiverAndroid>(transact_cb);
}

binder_status_t InputParcelAndroid::GetDataPosition() const {
  return AParcel_getDataPosition(parcel_);
}

binder_status_t InputParcelAndroid::SetDataPosition(int32_t pos) {
  return AParcel_setDataPosition(parcel_, pos);
}

binder_status_t InputParcelAndroid::WriteInt32(int32_t data) {
  return AParcel_writeInt32(parcel_, data);
}

binder_status_t InputParcelAndroid::WriteBinder(HasRawBinder* binder) {
  return AParcel_writeStrongBinder(
      parcel_, reinterpret_cast<AIBinder*>(binder->GetRawBinder()));
}

binder_status_t InputParcelAndroid::WriteString(absl::string_view s) {
  return AParcel_writeString(parcel_, s.data(), s.length());
}

binder_status_t InputParcelAndroid::WriteByteArray(const int8_t* buffer,
                                                   int32_t length) {
  return AParcel_writeByteArray(parcel_, buffer, length);
}

binder_status_t OutputParcelAndroid::ReadInt32(int32_t* data) const {
  return AParcel_readInt32(parcel_, data);
}

binder_status_t OutputParcelAndroid::ReadBinder(
    std::unique_ptr<Binder>* data) const {
  AIBinder* binder;
  int32_t res = AParcel_readStrongBinder(parcel_, &binder);
  *data = std::make_unique<BinderAndroid>(ndk::SpAIBinder(binder));
  return res;
}

namespace {

bool byte_array_allocator(void* arrayData, int32_t length, int8_t** outBuffer) {
  std::string tmp;
  tmp.resize(length);
  *reinterpret_cast<std::string*>(arrayData) = tmp;
  *outBuffer = reinterpret_cast<int8_t*>(
      reinterpret_cast<std::string*>(arrayData)->data());
  return true;
}

bool string_allocator(void* stringData, int32_t length, char** outBuffer) {
  if (length > 0) {
    GPR_ASSERT(length < 100);  // call should preallocate 100 bytes
    *outBuffer = reinterpret_cast<char*>(stringData);
  }
  return true;
}

}  // namespace

binder_status_t OutputParcelAndroid::ReadByteArray(std::string* data) const {
  return AParcel_readByteArray(parcel_, data, byte_array_allocator);
}

binder_status_t OutputParcelAndroid::ReadString(char data[111]) const {
  return AParcel_readString(parcel_, data, string_allocator);
}

}  // namespace binder_transport

#endif
