#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "src/core/ext/transport/binder/wire_format/binder_constants.h"

namespace binder_transport {

class HasRawBinder {
 public:
  virtual ~HasRawBinder() = default;
  virtual void* GetRawBinder() = 0;
};

class Binder;

// TODO(waynetu): We might need other methods as well.
// TODO(waynetu): Find a better way to express the returned status than
// binder_status_t.
class InputParcelInterface {
 public:
  virtual ~InputParcelInterface() = default;
  virtual binder_status_t GetDataPosition() const = 0;
  virtual binder_status_t SetDataPosition(int32_t pos) = 0;
  virtual binder_status_t WriteInt32(int32_t data) = 0;
  virtual binder_status_t WriteBinder(HasRawBinder* binder) = 0;
  virtual binder_status_t WriteString(absl::string_view s) = 0;
  virtual binder_status_t WriteByteArray(const int8_t* buffer,
                                         int32_t length) = 0;

  binder_status_t WriteByteArrayWithLength(absl::string_view buffer) {
    binder_status_t status = WriteInt32(buffer.length());
    if (status != STATUS_OK || buffer.empty()) return status;
    return WriteByteArray(reinterpret_cast<const int8_t*>(buffer.data()),
                          buffer.length());
  }
};

// TODO(waynetu): We might need other methods as well.
// TODO(waynetu): Find a better way to express the returned status than
// binder_status_t.
class OutputParcelInterface {
 public:
  virtual ~OutputParcelInterface() = default;
  virtual binder_status_t ReadInt32(int32_t* data) const = 0;
  virtual binder_status_t ReadBinder(std::unique_ptr<Binder>* data) const = 0;
  // TODO(waynetu): Provide better interfaces.
  virtual binder_status_t ReadByteArray(std::string* data) const = 0;
  // FIXME(waynetu): This is just a temporary interface.
  virtual binder_status_t ReadString(char data[111]) const = 0;
};

class TransactionReceiver : public HasRawBinder {
 public:
  using OnTransactCb = std::function<binder_status_t(
      transaction_code_t, const OutputParcelInterface*)>;

  ~TransactionReceiver() override = default;
};

class Binder : public HasRawBinder {
 public:
  ~Binder() override = default;

  virtual void Initialize() = 0;
  virtual binder_status_t PrepareTransaction() = 0;
  virtual binder_status_t Transact(BinderTransportTxCode tx_code,
                                   binder_flags_t flag) = 0;

  virtual InputParcelInterface* GetInputParcel() const = 0;
  virtual OutputParcelInterface* GetOutputParcel() const = 0;

  // TODO(waynetu): Can we decouple the receiver from the binder?
  virtual std::unique_ptr<TransactionReceiver> ConstructTxReceiver(
      TransactionReceiver::OnTransactCb transact_cb) const = 0;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_H_
