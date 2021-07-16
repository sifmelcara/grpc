#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_IMPL_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_IMPL_H_

#include <functional>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "src/core/ext/transport/binder/utils/transport_stream_receiver_interface.h"

// TODO(mingcl): find better folder for this class
namespace binder_transport {

// Routes the data received from transport to corresponding streams
class TransportStreamReceiverImpl : public TransportStreamReceiverInterface {
 public:
  void RegisterRecvInitialMd(StreamIdentifier id,
                             std::function<void(const Metadata&)> cb) override;
  void RegisterRecvMessage(StreamIdentifier id,
                           std::function<void(const std::string&)> cb) override;
  void RegisterRecvTrailingMd(
      StreamIdentifier id,
      std::function<void(const Metadata&, int)> cb) override;
  void NotifyRecvInitialMd(StreamIdentifier id,
                           const Metadata& initial_md) override;
  void NotifyRecvMessage(StreamIdentifier id,
                         const std::string& message) override;
  void NotifyRecvTrailingMd(StreamIdentifier id, const Metadata& trailing_md,
                            int status) override;

 private:
  std::map<StreamIdentifier, std::function<void(const Metadata&)>>
      initial_md_cbs_;
  std::map<StreamIdentifier, std::function<void(const std::string&)>>
      message_cbs_;
  std::map<StreamIdentifier, std::function<void(const Metadata&, int)>>
      trailing_md_cbs_;
  // TODO(waynetu): Better thread safety design. For example, use separate
  // mutexes for different type of messages.
  absl::Mutex m_;
  // TODO(waynetu): gRPC surface layer will not wait for the current message to
  // be delivered before sending the next message. The following implementation
  // is still buggy with the current implementation of wire writer if
  // transaction issued first completes after the one issued later does. This is
  // because we just take the first element out of the queue and assume it's the
  // one issued first without further checking, which results in callbacks being
  // invoked with incorrect data.
  //
  // This should be fixed in the wire writer level and make sure out-of-order
  // messages will be re-ordered by it. In such case, the queueing approach will
  // work fine. Refer to the TODO in WireWriterImpl::ProcessTransaction() at
  // wire_writer_impl.cc for detecting and resolving out-of-order transactions.
  std::map<StreamIdentifier, std::queue<Metadata>> pending_initial_md_;
  std::map<StreamIdentifier, std::queue<std::string>> pending_message_;
  std::map<StreamIdentifier, std::queue<std::pair<Metadata, int>>>
      pending_trailing_md_;
};
}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_IMPL_H_
