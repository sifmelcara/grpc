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


#include <grpc/support/port_platform.h>

#include <grpc/support/alloc.h>
#include <grpc/support/string_util.h>

#include "src/core/ext/filters/client_channel/resolver_registry.h"
#include "src/core/ext/filters/client_channel/server_address.h"
#include "src/core/lib/address_utils/parse_address.h"
#include "src/core/lib/channel/channel_args.h"
#include "src/core/lib/gpr/string.h"
#include "src/core/lib/iomgr/resolve_address.h"
#include "src/core/lib/iomgr/unix_sockets_posix.h"

namespace grpc_core {
namespace {

class BinderResolver : public Resolver {
 public:
  BinderResolver(ServerAddressList addresses, ResolverArgs args)
      : result_handler_(std::move(args.result_handler)),
        addresses_(std::move(addresses)),
        channel_args_(grpc_channel_args_copy(args.args)) {}

  ~BinderResolver() override { grpc_channel_args_destroy(channel_args_); };

  void StartLocked() override {
    Result result;
    result.addresses = std::move(addresses_);
    result.args = channel_args_;
    channel_args_ = nullptr;
    result_handler_->ReturnResult(std::move(result));
  }

  void ShutdownLocked() override {}

 private:
  std::unique_ptr<ResultHandler> result_handler_;
  ServerAddressList addresses_;
  const grpc_channel_args* channel_args_ = nullptr;
};

grpc_error_handle BinderAddrPopulate(absl::string_view authority,
                                     grpc_resolved_address* resolved_addr) {
  if (std::string(authority).size() + 1 > sizeof(resolved_addr->addr)) {
    return GRPC_ERROR_CREATE_FROM_CPP_STRING(std::string(authority) +
                                             " is too long to be handled");
  }
  if (std::find_if(authority.begin(), authority.end(),
                   [](char c) { return !isalnum(c); }) != authority.end()) {
    return GRPC_ERROR_CREATE_FROM_CPP_STRING(std::string(authority) +
                                             " contains invalid character");
  }
  strcpy(resolved_addr->addr, std::string(authority).c_str());
  resolved_addr->len = strlen(resolved_addr->addr) + 1;
  return GRPC_ERROR_NONE;
}

bool parse_binder(const grpc_core::URI& uri,
                  grpc_resolved_address* resolved_addr) {
  if (uri.scheme() != "binder") {
    gpr_log(GPR_ERROR, "Expected 'binder' scheme, got '%s'",
            uri.scheme().c_str());
    return false;
  }
  if (uri.authority().empty()) {
    gpr_log(GPR_ERROR, "authority missing in binder scheme");
    return false;
  }
  grpc_error_handle error = BinderAddrPopulate(uri.authority(), resolved_addr);
  if (error != GRPC_ERROR_NONE) {
    gpr_log(GPR_ERROR, "%s", grpc_error_std_string(error).c_str());
    GRPC_ERROR_UNREF(error);
    return false;
  }
  return true;
}

bool ParseUri(const URI& uri, ServerAddressList* addresses) {
  grpc_resolved_address addr;
  if (!parse_binder(uri, &addr)) {
    return false;
  }
  if (addresses != nullptr) {
    addresses->emplace_back(addr, nullptr /* args */);
  }
  return true;
}

class BinderResolverFactory : public ResolverFactory {
 public:
  bool IsValidUri(const URI& uri) const override {
    return ParseUri(uri, nullptr);
  }

  OrphanablePtr<Resolver> CreateResolver(ResolverArgs args) const override {
    ServerAddressList addresses;
    if (!ParseUri(args.uri, &addresses)) return nullptr;
    return MakeOrphanable<BinderResolver>(std::move(addresses),
                                          std::move(args));
  }

  const char* scheme() const override { return "binder"; }
};

}  // namespace
}  // namespace grpc_core

void grpc_resolver_binder_init() {
  grpc_core::ResolverRegistry::Builder::RegisterResolverFactory(
      absl::make_unique<grpc_core::BinderResolverFactory>());
}

void grpc_resolver_binder_shutdown() {}
