#include "src/core/ext/transport/binder/wire_format/server_binder.h"

#include <grpc/support/log.h>

#include <utility>

#include "absl/strings/str_cat.h"

namespace binder_transport {
ServerBinderImpl::ServerBinderImpl(std::unique_ptr<Binder> binder)
    : binder_(std::move(binder)) {}

void ServerBinderImpl::RpcCall(const Transaction& tx) {
  // TODO(mingcl): check tx_code <= last call id
  GPR_ASSERT(tx.GetTxCode() >= kFirstCallId);
  gpr_log(GPR_INFO, "rpc call prepare transaction = %d",
          binder_->PrepareTransaction());
  InputParcelInterface* input = binder_->GetInputParcel();
  {
    //  fill parcel
    input->WriteInt32(tx.GetFlags());
    input->WriteInt32(tx.GetSeqNum());
    if (tx.GetFlags() & 0x1) {
      // prefix set
      input->WriteString(tx.GetMethodRef());
      input->WriteInt32(tx.GetPrefixMds().size());
      for (const auto& md : tx.GetPrefixMds()) {
        input->WriteByteArrayWithLength(md.first);
        input->WriteByteArrayWithLength(md.second);
      }
    }
    if (tx.GetFlags() & 0x2) {
      input->WriteByteArrayWithLength(tx.GetMessageData());
    }
    if (tx.GetFlags() & 0x4) {
      // client suffix currently is always empty according to the wireformat
    }
  }
  // FIXME(waynetu): Construct BinderTransportTxCode from an arbitrary integer
  // is an undefined behavior.
  gpr_log(
      GPR_INFO, "AIBinder_transact = %d",
      binder_->Transact(BinderTransportTxCode(tx.GetTxCode()), FLAG_ONEWAY));
}
}  // namespace binder_transport
