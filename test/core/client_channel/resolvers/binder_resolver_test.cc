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

#include "src/core/lib/iomgr/port.h"

#ifdef GRPC_HAVE_UNIX_SOCKET
#include <sys/un.h>
#endif

#include <string.h>

#include <gtest/gtest.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/string_util.h>

#include "src/core/ext/filters/client_channel/resolver_registry.h"
#include "src/core/lib/channel/channel_args.h"
#include "test/core/util/test_config.h"

// Registers the factory with `grpc_core::ResolverRegistry`. Defined in
// binder_resolver.cc
void grpc_resolver_binder_init(void);

namespace {

class BinderResolverTest : public ::testing::Test {
 public:
  BinderResolverTest() {
    factory_ = grpc_core::ResolverRegistry::LookupResolverFactory("binder");
  }
  ~BinderResolverTest() override {}
  static void SetUpTestSuite() {
    grpc_init();
    if (grpc_core::ResolverRegistry::LookupResolverFactory("binder") ==
        nullptr) {
      // Binder resolver will only be registered on platforms that support
      // binder transport. If it is not registered on current platform, we
      // manually register it here for testing purpose.
      grpc_resolver_binder_init();
      ASSERT_TRUE(grpc_core::ResolverRegistry::LookupResolverFactory("binder"));
    }
  }
  static void TearDownTestSuite() { grpc_shutdown(); }

  void SetUp() override { ASSERT_TRUE(factory_); }

  class ResultHandler : public grpc_core::Resolver::ResultHandler {
   public:
    ResultHandler() : expect_result_(false) {}

    ResultHandler(const std::string& expected_binder_id)
        : expect_result_(true), expected_binder_id_(expected_binder_id) {}

    void ReturnResult(grpc_core::Resolver::Result result) override {
      EXPECT_TRUE(expect_result_);
      ASSERT_TRUE(result.addresses.size() == 1);
      grpc_core::ServerAddress addr = result.addresses[0];
      const struct sockaddr_un* un =
          reinterpret_cast<const struct sockaddr_un*>(addr.address().addr);
      EXPECT_EQ(addr.address().len,
                sizeof(un->sun_family) + expected_binder_id_.length() + 1);
      EXPECT_EQ(un->sun_family, AF_MAX);
      EXPECT_EQ(un->sun_path, expected_binder_id_);
    }

    void ReturnError(grpc_error_handle error) override {
      GRPC_ERROR_UNREF(error);
    }

   private:
    // Whether we expect ReturnResult function to be invoked
    bool expect_result_;

    std::string expected_binder_id_;
  };

  void TestSucceeds(const char* string, const std::string& expected_path) {
    gpr_log(GPR_DEBUG, "test: '%s' should be valid for '%s'", string,
            factory_->scheme());
    grpc_core::ExecCtx exec_ctx;
    absl::StatusOr<grpc_core::URI> uri = grpc_core::URI::Parse(string);
    ASSERT_TRUE(uri.ok()) << uri.status().ToString();
    grpc_core::ResolverArgs args;
    args.uri = std::move(*uri);
    args.result_handler =
        absl::make_unique<BinderResolverTest::ResultHandler>(expected_path);
    grpc_core::OrphanablePtr<grpc_core::Resolver> resolver =
        factory_->CreateResolver(std::move(args));
    ASSERT_TRUE(resolver != nullptr);
    resolver->StartLocked();
  }

  void TestFails(const char* string) {
    gpr_log(GPR_DEBUG, "test: '%s' should be invalid for '%s'", string,
            factory_->scheme());
    grpc_core::ExecCtx exec_ctx;
    absl::StatusOr<grpc_core::URI> uri = grpc_core::URI::Parse(string);
    ASSERT_TRUE(uri.ok()) << uri.status().ToString();
    grpc_core::ResolverArgs args;
    args.uri = std::move(*uri);
    args.result_handler =
        absl::make_unique<BinderResolverTest::ResultHandler>();
    grpc_core::OrphanablePtr<grpc_core::Resolver> resolver =
        factory_->CreateResolver(std::move(args));
    EXPECT_TRUE(resolver == nullptr);
  }

 private:
  grpc_core::ResolverFactory* factory_;
};

}  // namespace

TEST_F(BinderResolverTest, WrongScheme) {
  TestFails("bonder:10.2.1.1");
  TestFails("http:google.com");
}

// Authority is not allowed
TEST_F(BinderResolverTest, AuthorityPresents) {
  TestFails("binder://example");
  TestFails("binder://google.com");
  TestFails("binder://google.com/test");
}

// Path cannot be empty
TEST_F(BinderResolverTest, EmptyPath) {
  TestFails("binder:");
  TestFails("binder:/");
  TestFails("binder://");
}

// This test is hard coded and assumed that available space is 108 bytes
TEST_F(BinderResolverTest, PathLength) {
  // Length of 102 bytes should be fine
  TestSucceeds(("binder:l" + std::string(100, 'o') + "g").c_str(),
               "l" + std::string(100, 'o') + "g");

  // Length of 108 bytes (including null terminator) should be fine
  TestSucceeds(("binder:l" + std::string(105, 'o') + "g").c_str(),
               "l" + std::string(105, 'o') + "g");

  // Length of 109 bytes (including null terminator) should fail
  TestFails(("binder:l" + std::string(106, 'o') + "g").c_str());

  TestFails(("binder:l" + std::string(200, 'o') + "g").c_str());
}

TEST_F(BinderResolverTest, SlashPrefixes) {
  TestFails("binder:////");
  TestSucceeds("binder:///test", "test");
  TestSucceeds("binder:////////test", "test");
}

TEST_F(BinderResolverTest, WhiteSpaces) {
  TestSucceeds("binder:te st", "te st");
  TestSucceeds("binder:  ", "  ");
}

TEST_F(BinderResolverTest, ValidCases) {
  TestSucceeds("binder:[[", "[[");
  TestSucceeds("binder:google!com", "google!com");
  TestSucceeds("binder:test/", "test/");
  TestSucceeds("binder:test:", "test:");

  TestSucceeds("binder:e", "e");
  TestSucceeds("binder:example", "example");
  TestSucceeds("binder:google.com", "google.com");
  TestSucceeds("binder:~", "~");
  TestSucceeds("binder:12345", "12345");
  TestSucceeds(
      "binder:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._"
      "~",
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  grpc::testing::TestEnvironment env(argc, argv);
  return RUN_ALL_TESTS();
}
