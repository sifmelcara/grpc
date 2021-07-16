#ifndef GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_INTERFACE_H_
#define GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_INTERFACE_H_

#include <functional>
#include <string>
#include <vector>

#include "src/core/ext/transport/binder/wire_format/transaction.h"

// TODO(mingcl): find better folder for this class
namespace binder_transport {

// TODO(mingcl): Define these alias in a better place
typedef int StreamIdentifier;

class TransportStreamReceiverInterface {
 public:
  virtual ~TransportStreamReceiverInterface() = default;

  // Only handles single time invocation. Callback object will be deleted.
  // The callback should be valid until invocation or unregister.
  virtual void RegisterRecvInitialMd(
      StreamIdentifier id, std::function<void(const Metadata&)> cb) = 0;
  // TODO(mingcl): Use string_view
  virtual void RegisterRecvMessage(
      StreamIdentifier id, std::function<void(const std::string&)> cb) = 0;
  virtual void RegisterRecvTrailingMd(
      StreamIdentifier id, std::function<void(const Metadata&, int)> cb) = 0;

  // TODO(mingcl): Provide a way to unregister callback?

  // TODO(mingcl): Figure out how to handle the case where there is no callback
  // registered for the stream. For now, I don't see this case happening in
  // unary calls. So we would probably just crash the program for now.
  // For streaming calls it does happen, for now we simply queue them.
  virtual void NotifyRecvInitialMd(StreamIdentifier id,
                                   const Metadata& initial_md) = 0;
  virtual void NotifyRecvMessage(StreamIdentifier id,
                                 const std::string& message) = 0;
  virtual void NotifyRecvTrailingMd(StreamIdentifier id,
                                    const Metadata& trailing_md,
                                    int status) = 0;
};

}  // namespace binder_transport

#endif  // GRPC_CORE_EXT_TRANSPORT_BINDER_UTILS_TRANSPORT_STREAM_RECEIVER_INTERFACE_H_
