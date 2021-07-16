#include "src/core/ext/transport/binder/utils/transport_stream_receiver_impl.h"

#include <grpc/support/log.h>

#include <functional>
#include <string>
#include <utility>

namespace binder_transport {
void TransportStreamReceiverImpl::RegisterRecvInitialMd(
    StreamIdentifier id, std::function<void(const Metadata&)> cb) {
  // TODO(mingcl): Don't lock the whole function
  absl::MutexLock l(&m_);
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  GPR_ASSERT(initial_md_cbs_.count(id) == 0);
  auto iter = pending_initial_md_.find(id);
  if (iter == pending_initial_md_.end()) {
    initial_md_cbs_[id] = std::move(cb);
  } else {
    cb(iter->second.front());
    iter->second.pop();
    if (iter->second.empty()) {
      pending_initial_md_.erase(iter);
    }
  }
}

void TransportStreamReceiverImpl::RegisterRecvMessage(
    StreamIdentifier id, std::function<void(const std::string&)> cb) {
  // TODO(mingcl): Don't lock the whole function
  absl::MutexLock l(&m_);
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  GPR_ASSERT(message_cbs_.count(id) == 0);
  auto iter = pending_message_.find(id);
  if (iter == pending_message_.end()) {
    message_cbs_[id] = std::move(cb);
  } else {
    cb(iter->second.front());
    iter->second.pop();
    if (iter->second.empty()) {
      pending_message_.erase(iter);
    }
  }
}

void TransportStreamReceiverImpl::RegisterRecvTrailingMd(
    StreamIdentifier id, std::function<void(const Metadata&, int)> cb) {
  // TODO(mingcl): Don't lock the whole function
  absl::MutexLock l(&m_);
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  GPR_ASSERT(trailing_md_cbs_.count(id) == 0);
  auto iter = pending_trailing_md_.find(id);
  if (iter == pending_trailing_md_.end()) {
    trailing_md_cbs_[id] = std::move(cb);
  } else {
    {
      const auto& p = iter->second.front();
      cb(p.first, p.second);
    }
    iter->second.pop();
    if (iter->second.empty()) {
      pending_trailing_md_.erase(iter);
    }
  }
}

void TransportStreamReceiverImpl::NotifyRecvInitialMd(
    StreamIdentifier id, const Metadata& initial_md) {
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  std::function<void(const Metadata&)> cb;
  {
    absl::MutexLock l(&m_);
    auto iter = initial_md_cbs_.find(id);
    if (iter != initial_md_cbs_.end()) {
      cb = iter->second;
      initial_md_cbs_.erase(iter);
    } else {
      pending_initial_md_[id].push(initial_md);
    }
  }
  if (cb != nullptr) {
    cb(initial_md);
  }
}

void TransportStreamReceiverImpl::NotifyRecvMessage(
    StreamIdentifier id, const std::string& message) {
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  std::function<void(const std::string&)> cb;
  {
    absl::MutexLock l(&m_);
    auto iter = message_cbs_.find(id);
    if (iter != message_cbs_.end()) {
      cb = iter->second;
      message_cbs_.erase(iter);
    } else {
      pending_message_[id].push(message);
    }
  }
  if (cb != nullptr) {
    cb(message);
  }
}

void TransportStreamReceiverImpl::NotifyRecvTrailingMd(
    StreamIdentifier id, const Metadata& trailing_md, int status) {
  gpr_log(GPR_ERROR, "%s id = %d", __func__, id);
  std::function<void(const Metadata&, int)> cb;
  {
    absl::MutexLock l(&m_);
    auto iter = trailing_md_cbs_.find(id);
    if (iter != trailing_md_cbs_.end()) {
      cb = iter->second;
      trailing_md_cbs_.erase(iter);
    } else {
      pending_trailing_md_[id].emplace(trailing_md, status);
    }
  }
  if (cb != nullptr) {
    cb(trailing_md, status);
  }
}
}  // namespace binder_transport
