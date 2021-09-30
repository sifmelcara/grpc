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

#include <string.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/string_util.h>

#include "src/core/ext/filters/client_channel/resolver_registry.h"
#include "src/core/lib/channel/channel_args.h"
#include "test/core/util/test_config.h"

namespace {

class ResultHandler : public grpc_core::Resolver::ResultHandler {
 public:
  void ReturnResult(grpc_core::Resolver::Result /*result*/) override {}

  void ReturnError(grpc_error_handle error) override {
    GRPC_ERROR_UNREF(error);
  }
};

void test_succeeds(grpc_core::ResolverFactory* factory, const char* string) {
  gpr_log(GPR_DEBUG, "test: '%s' should be valid for '%s'", string,
          factory->scheme());
  grpc_core::ExecCtx exec_ctx;
  absl::StatusOr<grpc_core::URI> uri = grpc_core::URI::Parse(string);
  if (!uri.ok()) {
    gpr_log(GPR_ERROR, "%s", uri.status().ToString().c_str());
    GPR_ASSERT(uri.ok());
  }
  grpc_core::ResolverArgs args;
  args.uri = std::move(*uri);
  args.result_handler = absl::make_unique<ResultHandler>();
  grpc_core::OrphanablePtr<grpc_core::Resolver> resolver =
      factory->CreateResolver(std::move(args));
  GPR_ASSERT(resolver != nullptr);
  resolver->StartLocked();
}

void test_fails(grpc_core::ResolverFactory* factory, const char* string) {
  gpr_log(GPR_DEBUG, "test: '%s' should be invalid for '%s'", string,
          factory->scheme());
  grpc_core::ExecCtx exec_ctx;
  absl::StatusOr<grpc_core::URI> uri = grpc_core::URI::Parse(string);
  if (!uri.ok()) {
    gpr_log(GPR_ERROR, "%s", uri.status().ToString().c_str());
    GPR_ASSERT(uri.ok());
  }
  grpc_core::ResolverArgs args;
  args.uri = std::move(*uri);
  args.result_handler = absl::make_unique<ResultHandler>();
  grpc_core::OrphanablePtr<grpc_core::Resolver> resolver =
      factory->CreateResolver(std::move(args));
  GPR_ASSERT(resolver == nullptr);
}
}  // namespace

// Registers the factory with `grpc_core::ResolverRegistry`. Defined in
// binder_resolver.cc
void grpc_resolver_binder_init(void);

int main(int argc, char** argv) {
  grpc::testing::TestEnvironment env(argc, argv);
  grpc_init();

  grpc_core::ResolverFactory* binder_factory =
      grpc_core::ResolverRegistry::LookupResolverFactory("binder");

  if (!binder_factory) {
    // Binder resolver will only be registered on platforms that support binder
    // transport. If it is not registered on current platform, we manually
    // register it here for testing purpose.
    grpc_resolver_binder_init();
    binder_factory =
        grpc_core::ResolverRegistry::LookupResolverFactory("binder");
    GPR_ASSERT(binder_factory);
  }

  // Wrong scheme
  test_fails(binder_factory, "bonder://10.2.1.1");
  test_fails(binder_factory, "http://google.com");

  // Empty authority is not allowed
  test_fails(binder_factory, "binder://");
  test_fails(binder_factory, "binder:example");

  // Cannot fit into typical socket path size (128 bytes)
  test_fails(binder_factory,
             ("binder://l" + std::string(200, 'o') + "g").c_str());

  // Contains invalid character
  test_fails(binder_factory, "binder://///");
  test_fails(binder_factory, "binder://[[[[");
  test_fails(binder_factory, "binder://google.com");
  test_fails(binder_factory, "binder://aaaa,bbbb");

  test_succeeds(binder_factory, "binder://example");
  test_succeeds(binder_factory, "binder://example123");
  test_succeeds(binder_factory, "binder://ExaMpLe123");
  test_succeeds(binder_factory, "binder://12345");

  grpc_shutdown();
  return 0;
}
