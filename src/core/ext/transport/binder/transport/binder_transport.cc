#include "src/core/ext/transport/binder/transport/binder_transport.h"

#include <grpc/support/log.h>
#include <grpc/support/port_platform.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "src/core/ext/transport/binder/transport/binder_stream.h"
#include "src/core/ext/transport/binder/utils/transport_stream_receiver_impl.h"
#include "src/core/ext/transport/binder/utils/transport_stream_receiver_interface.h"
#include "src/core/ext/transport/binder/wire_format/server_binder.h"
#include "src/core/ext/transport/binder/wire_format/wire_writer.h"
#include "src/core/ext/transport/binder/wire_format/wire_writer_impl.h"
#include "src/core/lib/iomgr/exec_ctx.h"
#include "src/core/lib/slice/slice_utils.h"
#include "src/core/lib/transport/byte_stream.h"
#include "src/core/lib/transport/metadata_batch.h"
#include "src/core/lib/transport/static_metadata.h"
#include "src/core/lib/transport/transport.h"

static int init_stream(grpc_transport* gt, grpc_stream* gs,
                       grpc_stream_refcount* refcount, const void* server_data,
                       grpc_core::Arena* arena) {
  GPR_TIMER_SCOPE("init_stream", 0);
  gpr_log(GPR_INFO, "%s = %p %p %p %p %p", __func__, gt, gs, refcount,
          server_data, arena);
  grpc_binder_transport* t = reinterpret_cast<grpc_binder_transport*>(gt);
  // TODO(mingcl): Figure out if we need to worry about concurrent invocation
  // here
  new (gs) grpc_binder_stream(t, t->NewStreamTxCode());
  return 0;
}

static void set_pollset(grpc_transport* gt, grpc_stream* gs, grpc_pollset* gp) {
  gpr_log(GPR_INFO, "%s = %p %p %p", __func__, gt, gs, gp);
}

static void set_pollset_set(grpc_transport*, grpc_stream*, grpc_pollset_set*) {
  gpr_log(GPR_INFO, __func__);
}

void AssignMd(grpc_metadata_batch* mb,
              std::vector<std::pair<std::string, std::string>> md) {
  grpc_metadata_batch_init(mb);
  auto* persistant_md = new decltype(md);
  *persistant_md = md;
  for (auto& p : md) {
    grpc_linked_mdelem* glm = new grpc_linked_mdelem;
    memset(glm, 0, sizeof(grpc_linked_mdelem));
    glm->md =
        grpc_mdelem_from_slices(grpc_slice_intern(grpc_slice_from_static_buffer(
                                    p.first.data(), p.first.length())),
                                grpc_slice_intern(grpc_slice_from_static_buffer(
                                    p.second.data(), p.second.length())));
    GPR_ASSERT(grpc_metadata_batch_link_tail(mb, glm) == GRPC_ERROR_NONE);
  }
}

static void perform_stream_op(grpc_transport* gt, grpc_stream* gs,
                              grpc_transport_stream_op_batch* op) {
  GPR_TIMER_SCOPE("perform_stream_op", 0);
  gpr_log(GPR_INFO, "%s = %p %p %p", __func__, gt, gs, op);
  grpc_binder_transport* gbt = reinterpret_cast<grpc_binder_transport*>(gt);
  grpc_binder_stream* gbs = reinterpret_cast<grpc_binder_stream*>(gs);
  binder_transport::Transaction tx(
      /*tx_code=*/gbs->GetTxCode(), /*seq_num=*/gbs->GetThenIncSeq());
  if (op->send_initial_metadata) {
    gpr_log(GPR_INFO, "send_initial_metadata");
    std::vector<std::pair<std::string, std::string>> init_md;
    std::string path;
    auto batch = op->payload->send_initial_metadata.send_initial_metadata;

    for (grpc_linked_mdelem* md = batch->list.head; md != nullptr;
         md = md->next) {
      absl::string_view key =
          grpc_core::StringViewFromSlice(GRPC_MDKEY(md->md));
      absl::string_view value =
          grpc_core::StringViewFromSlice(GRPC_MDVALUE(md->md));
      gpr_log(GPR_INFO, "send initial metatday key-value %s",
              absl::StrCat(key, " ", value).c_str());
      if (key == ":path") {
        // TODO(b/192208403): Figure out if it is correct to simply drop '/'
        // prefix and treat it as rpc method name
        GPR_ASSERT(value[0] == '/');
        path = std::string(value).substr(1);
      } else {
        init_md.push_back({std::string(key), std::string(value)});
      }
    }
    tx.SetPrefix(path, init_md);
  }
  if (op->send_message) {
    gpr_log(GPR_INFO, "send_message");
    grpc_slice s;
    bool next_result =
        op->payload->send_message.send_message->Next(SIZE_MAX, nullptr);
    gpr_log(GPR_INFO, "next_result = %d", static_cast<int>(next_result));
    op->payload->send_message.send_message->Pull(&s);
    auto* p = GRPC_SLICE_START_PTR(s);
    int len = GRPC_SLICE_LENGTH(s);
    std::string message_data(reinterpret_cast<char*>(p), len);
    gpr_log(GPR_INFO, "message_data = %s", message_data.c_str());
    tx.SetData(message_data);
    // TODO(b/192369787): Are we supposed to reset here to avoid use-after-free
    // issue in call.cc?
    op->payload->send_message.send_message.reset();
  }
  if (op->send_trailing_metadata) {
    gpr_log(GPR_INFO, "send_trailing_metadata");
    auto batch = op->payload->send_trailing_metadata.send_trailing_metadata;

    for (grpc_linked_mdelem* md = batch->list.head; md != nullptr;
         md = md->next) {
      absl::string_view key =
          grpc_core::StringViewFromSlice(GRPC_MDKEY(md->md));
      absl::string_view value =
          grpc_core::StringViewFromSlice(GRPC_MDVALUE(md->md));
      gpr_log(GPR_INFO, "WARNING send trailing metatday key-value %s",
              absl::StrCat(key, " ", value).c_str());
    }
    // TODO(mingcl): Will we ever has key-value pair here? According to
    // wireformat client suffix data is always empty.
    tx.SetSuffix();
  }
  if (op->recv_initial_metadata) {
    gpr_log(GPR_INFO, "recv_initial_metadata");
    gbt->transport_stream_receiver->RegisterRecvInitialMd(
        gbs->tx_code,
        [cb = op->payload->recv_initial_metadata.recv_initial_metadata_ready,
         out = op->payload->recv_initial_metadata.recv_initial_metadata](
            const binder_transport::Metadata& initial_md) {
          grpc_core::ExecCtx exec_ctx;
          GPR_ASSERT(out);
          GPR_ASSERT(cb);
          AssignMd(out, initial_md);
          grpc_core::ExecCtx::Run(DEBUG_LOCATION, cb, GRPC_ERROR_NONE);
          gpr_log(GPR_INFO, __func__);
        });
  }
  if (op->recv_message) {
    gpr_log(GPR_INFO, "recv_message");
    gbt->transport_stream_receiver->RegisterRecvMessage(
        gbs->tx_code, [cb = op->payload->recv_message.recv_message_ready,
                       out = op->payload->recv_message.recv_message](
                          const std::string& message) {
          grpc_core::ExecCtx exec_ctx;
          GPR_ASSERT(out);
          GPR_ASSERT(cb);
          grpc_slice_buffer buf;
          grpc_slice_buffer_init(&buf);
          auto* persistent_msg = new std::string;
          *persistent_msg = message;
          grpc_slice_buffer_add(
              &buf, grpc_slice_from_static_buffer(persistent_msg->data(),
                                                  persistent_msg->length()));
          (*out) = grpc_core::OrphanablePtr<grpc_core::ByteStream>(
              new grpc_core::SliceBufferByteStream(&buf, /*flags =*/0));
          grpc_core::ExecCtx::Run(DEBUG_LOCATION, cb, GRPC_ERROR_NONE);
          gpr_log(GPR_INFO, __func__);
        });
  }
  if (op->recv_trailing_metadata) {
    gpr_log(GPR_INFO, "recv_trailing_metadata");
    gbt->transport_stream_receiver->RegisterRecvTrailingMd(
        gbs->tx_code,
        [cb = op->payload->recv_trailing_metadata.recv_trailing_metadata_ready,
         out = op->payload->recv_trailing_metadata.recv_trailing_metadata](
            const binder_transport::Metadata& trailing_md, int status) {
          grpc_core::ExecCtx exec_ctx;
          GPR_ASSERT(out);
          GPR_ASSERT(cb);
          AssignMd(out, trailing_md);
          {
            // Append status to metadata
            // TODO(b/192208695): Figure out why gRPC's upper layer is designed
            // like this
            std::string str = std::to_string(status);
            grpc_linked_mdelem* glm = new grpc_linked_mdelem;
            memset(glm, 0, sizeof(grpc_linked_mdelem));
            glm->md = grpc_mdelem_from_slices(
                GRPC_MDSTR_GRPC_STATUS,
                grpc_core::UnmanagedMemorySlice(str.c_str()));
            GPR_ASSERT(grpc_metadata_batch_link_tail(out, glm) ==
                       GRPC_ERROR_NONE);
            gpr_log(GPR_ERROR, "trailing_md = %p", out);
            gpr_log(GPR_ERROR, "glm = %p", glm);
            // gpr_log(GPR_INFO, "grpc_status ptr = %d",
            // static_cast<int>(out->idx.named.grpc_status));
          }
          grpc_core::ExecCtx::Run(DEBUG_LOCATION, cb, GRPC_ERROR_NONE);
          gpr_log(GPR_INFO, __func__);
        });
  }
  if (op->cancel_stream) {
    gpr_log(GPR_INFO, "cancel_stream");
  }
  gbt->server_binder->RpcCall(tx);
  // Note that this should only be scheduled when all non-recv ops are completed
  if (op->on_complete != nullptr) {
    grpc_core::ExecCtx::Run(DEBUG_LOCATION, op->on_complete, GRPC_ERROR_NONE);
    gpr_log(GPR_INFO, "on_complete closure schuduled");
  }
}

static void perform_transport_op(grpc_transport* gt, grpc_transport_op* op) {
  gpr_log(GPR_INFO, __func__);
}

static void destroy_stream(grpc_transport* gt, grpc_stream* gs,
                           grpc_closure* then_schedule_closure) {
  gpr_log(GPR_INFO, __func__);
}

static void destroy_transport(grpc_transport* gt) {
  gpr_log(GPR_INFO, __func__);
}

static grpc_endpoint* get_endpoint(grpc_transport*) {
  gpr_log(GPR_INFO, __func__);
  return nullptr;
}

// See grpc_transport_vtable declaration for meaning of each field
static const grpc_transport_vtable vtable = {sizeof(grpc_binder_stream),
                                             "binder",
                                             init_stream,
                                             set_pollset,
                                             set_pollset_set,
                                             perform_stream_op,
                                             perform_transport_op,
                                             destroy_stream,
                                             destroy_transport,
                                             get_endpoint};

static const grpc_transport_vtable* get_vtable() { return &vtable; }

grpc_binder_transport::grpc_binder_transport(
    std::unique_ptr<binder_transport::Binder> endpoint_binder) {
  gpr_log(GPR_INFO, __func__);
  base.vtable = get_vtable();
  transport_stream_receiver =
      std::make_unique<binder_transport::TransportStreamReceiverImpl>();
  wire_writer = std::make_unique<binder_transport::WireWriterImpl>(
      transport_stream_receiver.get());
  auto transport_pair = wire_writer->SetupTransport(std::move(endpoint_binder));
  server_binder = std::move(transport_pair.first);
  tx_receiver = std::move(transport_pair.second);
}

grpc_transport* grpc_create_binder_transport(
    std::unique_ptr<binder_transport::Binder> endpoint_binder) {
  gpr_log(GPR_INFO, __func__);

  grpc_binder_transport* t =
      new grpc_binder_transport(std::move(endpoint_binder));

  return &t->base;
}
