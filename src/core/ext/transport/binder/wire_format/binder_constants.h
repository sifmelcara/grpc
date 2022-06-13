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

#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H
#define GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H

#include <grpc/support/port_platform.h>

#include <cstdint>

#include "absl/base/attributes.h"

using transaction_code_t = uint32_t;

ABSL_CONST_INIT extern const int FIRST_CALL_TRANSACTION;
ABSL_CONST_INIT extern const int LAST_CALL_TRANSACTION;

namespace grpc_binder {

struct BinderTransportTxCode {
  explicit BinderTransportTxCode(int code) : code(code){};
  bool operator==(const BinderTransportTxCode& other) const {
    return this->code == other.code;
  }
  int code;
  static constexpr int SETUP_TRANSPORT = 1;
  static constexpr int SHUTDOWN_TRANSPORT = 2;
  static constexpr int ACKNOWLEDGE_BYTES = 3;
  static constexpr int PING = 4;
  static constexpr int PING_RESPONSE = 5;
};

ABSL_CONST_INIT extern const int kFirstCallId;

}  // namespace grpc_binder

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_WIRE_FORMAT_BINDER_CONSTANTS_H
