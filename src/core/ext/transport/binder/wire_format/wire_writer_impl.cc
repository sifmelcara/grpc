#include "src/core/ext/transport/binder/wire_format/wire_writer_impl.h"

#include <grpc/support/log.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "src/core/ext/transport/binder/utils/transport_stream_receiver_interface.h"
#include "src/core/ext/transport/binder/wire_format/binder.h"
#include "src/core/ext/transport/binder/wire_format/server_binder.h"

namespace {

bool operator==(int x, binder_transport::BinderTransportTxCode c) {
  return x == static_cast<int>(c);
}

std::vector<std::pair<std::string, std::string>> parse_metadata(
    const binder_transport::OutputParcelInterface* output) {
  int num_header;
  gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&num_header));
  gpr_log(GPR_INFO, "num_header = %d", num_header);
  std::vector<std::pair<std::string, std::string>> ret;
  for (int i = 0; i != num_header; i++) {
    int count;
    gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&count));
    gpr_log(GPR_INFO, "count = %d", count);
    std::string key{};
    if (count > 0) output->ReadByteArray(&key);
    gpr_log(GPR_INFO, "key = %s", key.c_str());
    gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&count));
    gpr_log(GPR_INFO, "count = %d", count);
    std::string value{};
    if (count > 0) output->ReadByteArray(&value);
    gpr_log(GPR_INFO, "value = %s", value.c_str());
    ret.push_back({key, value});
  }
  return ret;
}

}  // namespace

namespace binder_transport {

WireWriterImpl::WireWriterImpl(
    TransportStreamReceiverInterface* transport_stream_receiver)
    : transport_stream_receiver_(transport_stream_receiver) {}

std::pair<std::unique_ptr<ServerBinder>, std::unique_ptr<TransactionReceiver>>
WireWriterImpl::SetupTransport(std::unique_ptr<Binder> binder) {
  gpr_log(GPR_INFO, "Setting up transport");

  binder->Initialize();
  gpr_log(GPR_INFO, "prepare transaction = %d", binder->PrepareTransaction());
  InputParcelInterface* input = binder->GetInputParcel();
  gpr_log(GPR_INFO, "data position = %d", input->GetDataPosition());
  // gpr_log(GPR_INFO, "set data position to 0 = %d",
  // input->SetDataPosition(0));
  gpr_log(GPR_INFO, "data position = %d", input->GetDataPosition());
  int32_t version = 77;
  gpr_log(GPR_INFO, "write int32 = %d", input->WriteInt32(version));
  gpr_log(GPR_INFO, "data position = %d", input->GetDataPosition());

  // The lifetime of the transaction receiver is the same as the wire writer's.
  // The transaction receiver is responsible for not calling the on-transact
  // callback when it's dead.
  std::unique_ptr<TransactionReceiver> tx_receiver =
      binder->ConstructTxReceiver(
          [this](transaction_code_t code, const OutputParcelInterface* output) {
            return this->ProcessTransaction(code, output);
          });

  gpr_log(GPR_INFO, "tx_receiver = %p", tx_receiver->GetRawBinder());
  gpr_log(GPR_INFO, "AParcel_writeStrongBinder = %d",
          input->WriteBinder(tx_receiver.get()));
  gpr_log(
      GPR_INFO, "AIBinder_transact = %d",
      binder->Transact(BinderTransportTxCode::SETUP_TRANSPORT, FLAG_ONEWAY));

  // TODO(b/191941760): avoid blocking, handle server_binder_noti lifetime
  // better
  gpr_log(GPR_INFO, "start waiting for noti");
  server_binder_noti_.WaitForNotification();
  gpr_log(GPR_INFO, "end waiting for noti");
  return {std::make_unique<ServerBinderImpl>(std::move(server_binder_)),
          std::move(tx_receiver)};
}

binder_status_t WireWriterImpl::ProcessTransaction(
    transaction_code_t code,
    const binder_transport::OutputParcelInterface* output) {
  gpr_log(GPR_INFO, __func__);
  gpr_log(GPR_INFO, "tx code = %u", code);
  if (code == BinderTransportTxCode::SETUP_TRANSPORT) {
    // int datasize;
    int version;
    // getDataSize not supported until 31
    // gpr_log(GPR_INFO, "getDataSize = %d", AParcel_getDataSize(in,
    // &datasize));
    gpr_log(GPR_INFO, "readInt32 = %d", output->ReadInt32(&version));
    // gpr_log(GPR_INFO, "data size = %d", datasize);
    gpr_log(GPR_INFO, "version = %d", version);
    std::unique_ptr<Binder> binder{};
    {
      gpr_log(GPR_INFO, "AParcel_readStrongBinder = %d",
              output->ReadBinder(&binder));
      // gpr_log(GPR_INFO, "server_binder = %p", tmp);
    }
    binder->Initialize();
    server_binder_ = std::move(binder);
    server_binder_noti_.Notify();
  } else if (code == BinderTransportTxCode::PING_RESPONSE) {
    int value = -1;
    gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&value));
    gpr_log(GPR_INFO, "received ping response = %d", value);
  } else if (code >= binder_transport::kFirstCallId) {
    gpr_log(GPR_INFO, "This is probably a Streaming Tx");
    int flags;
    gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&flags));
    gpr_log(GPR_INFO, "flags = %d", flags);
    int status = flags >> 16;
    gpr_log(GPR_INFO, "status = %d", status);
    gpr_log(GPR_INFO, "FLAG_PREFIX = %d", (flags & kFlagPrefix));
    gpr_log(GPR_INFO, "FLAG_MESSAGE_DATA = %d", (flags & kFlagMessageData));
    gpr_log(GPR_INFO, "FLAG_SUFFIX = %d", (flags & kFlagSuffix));
    int seq_num;
    gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&seq_num));
    // TODO(waynetu): For now we'll just assume that the transactions commit in
    // the same order they're issued. The following assertion detects
    // out-of-order or missing transactions. WireWriterImpl should be fixed if
    // we indeed found such behavior.
    uint32_t& expectation = expected_seq_num_[code];
    GPR_ASSERT(seq_num == expectation && "Interleaved sequence number");
    expectation++;
    // TODO(waynetu): According to the protocol, "The sequence number will wrap
    // around to 0 if more than 2^31 messages are sent." For now we'll just
    // assert that it never reach such circumstances.
    GPR_ASSERT(expectation < (1U << 31) && "Sequence number too large");
    gpr_log(GPR_INFO, "sequence number = %d", seq_num);
    if (flags & kFlagPrefix) {
      transport_stream_receiver_->NotifyRecvInitialMd(code,
                                                      parse_metadata(output));
    }
    if (flags & kFlagMessageData) {
      int count;
      gpr_log(GPR_INFO, "read int32 = %d", output->ReadInt32(&count));
      gpr_log(GPR_INFO, "count = %d", count);
      std::string msg_data{};
      if (count > 0) output->ReadByteArray(&msg_data);
      gpr_log(GPR_INFO, "msg_data = %s", msg_data.c_str());
      transport_stream_receiver_->NotifyRecvMessage(code, msg_data);
    }
    if (flags & kFlagSuffix) {
      if (flags & kFlagStatusDescription) {
        // FLAG_STATUS_DESCRIPTION set
        char desc[111];
        output->ReadString(desc);
        gpr_log(GPR_INFO, "description = %s", desc);
      }
      transport_stream_receiver_->NotifyRecvTrailingMd(
          code, parse_metadata(output), status);
    }
  }
  return STATUS_OK;
}

}  // namespace binder_transport
