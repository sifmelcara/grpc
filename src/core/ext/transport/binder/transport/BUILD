# TODO(mingcl): describe this package.

load("//bazel:grpc_build_system.bzl", "grpc_cc_library")

licenses(["notice"])

grpc_cc_library(
    name = "transport",
    srcs = [
        "binder_transport.cc",
    ],
    hdrs = [
        "binder_stream.h",
        "binder_transport.h",
    ],
    external_deps = [
        "absl/strings",
    ],
    # To be fixed
    visibility = ["//visibility:public"],
    deps = [
        "//:gpr",
        "//:gpr_base",
        "//:grpc",
        "//:grpc++_base",
        "//:grpc_base",
        "//:grpc_base_c",
        "//:grpc_codegen",
        "//src/core/ext/transport/binder/utils:transport_stream_receiver",
        "//src/core/ext/transport/binder/wire_format:binder",
        "//src/core/ext/transport/binder/wire_format:server_binder",
        "//src/core/ext/transport/binder/wire_format:wire_writer",
    ],
)
