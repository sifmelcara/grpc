#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H_

#ifdef ANDROID

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>

#else

#include <cstdint>

using binder_status_t = int32_t;
using binder_flags_t = uint32_t;
using transaction_code_t = uint32_t;

constexpr binder_flags_t FLAG_ONEWAY = 0x01;
constexpr binder_status_t STATUS_OK = 0;

constexpr int FIRST_CALL_TRANSACTION = 0x00000001;
constexpr int LAST_CALL_TRANSACTION = 0x00FFFFFF;

#endif  // ANDROID

namespace binder_transport {

enum class BinderTransportTxCode {
  SETUP_TRANSPORT = 1,
  SHUTDOWN_TRANSPORT = 2,
  ACKNOWLEDGE_BYTES = 3,
  PING = 4,
  PING_RESPONSE = 5,
};

const int kFirstCallId = FIRST_CALL_TRANSACTION + 1000;

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H_
