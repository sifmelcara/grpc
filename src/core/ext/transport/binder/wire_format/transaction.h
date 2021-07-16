#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_TRANSACTION_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_TRANSACTION_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace binder_transport {

inline constexpr int kFlagPrefix = 0x1;
inline constexpr int kFlagMessageData = 0x2;
inline constexpr int kFlagSuffix = 0x4;
inline constexpr int kFlagStatusDescription = 0x20;

using Metadata = std::vector<std::pair<std::string, std::string>>;

class Transaction {
 public:
  Transaction(int tx_code, int seq_num)
      : tx_code_(tx_code), seq_num_(seq_num) {}
  // TODO(mingcl): Use string_view
  void SetPrefix(std::string method_ref, Metadata prefix_mds) {
    method_ref_ = method_ref;
    prefix_mds_ = prefix_mds;
    assert((flags_ & kFlagPrefix) == 0);
    flags_ |= kFlagPrefix;
  }
  void SetData(std::string message_data) {
    message_data_ = message_data;
    assert((flags_ & kFlagMessageData) == 0);
    flags_ |= kFlagMessageData;
  }
  void SetSuffix() {
    assert((flags_ & kFlagSuffix) == 0);
    flags_ |= kFlagSuffix;
  }

  int GetTxCode() const { return tx_code_; }
  int GetSeqNum() const { return seq_num_; }
  int GetFlags() const { return flags_; }

  absl::string_view GetMethodRef() const { return method_ref_; }
  const Metadata& GetPrefixMds() const { return prefix_mds_; }
  absl::string_view GetMessageData() const { return message_data_; }

 private:
  int tx_code_;
  int seq_num_;
  std::string method_ref_;
  Metadata prefix_mds_;
  std::string message_data_;
  // TODO(mingcl): Make this more solid
  int flags_ = 0;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_TRANSACTION_H_
