#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_STREAM_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_STREAM_H_

#include "src/core/ext/transport/binder/transport/binder_transport.h"

// TODO(mingcl): Figure out if we want to use class instead of struct here
struct grpc_binder_stream {
  grpc_binder_stream(grpc_binder_transport* t, int tx_code)
      : t(t), seq(0), tx_code(tx_code) {}
  ~grpc_binder_stream() = default;
  int GetTxCode() { return tx_code; }
  int GetThenIncSeq() { return seq++; }

  grpc_binder_transport* t;
  int seq;
  int tx_code;
};

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_TRANSPORT_BINDER_STREAM_H_
