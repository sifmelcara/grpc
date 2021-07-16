# TODO(mingcl): describe this package.

# TODO(mingcl): describe this package.
load("//bazel:grpc_build_system.bzl", "grpc_cc_library")

licenses(["notice"])

grpc_cc_library(
    name = "transport_stream_receiver",
    srcs = [
        "transport_stream_receiver_impl.cc",
    ],
    hdrs = [
        "transport_stream_receiver_impl.h",
        "transport_stream_receiver_interface.h",
    ],
    external_deps = [
        "absl/strings",
        "absl/synchronization",
        "absl/types:optional",
    ],
    # To be fixed
    visibility = ["//visibility:public"],
    deps = [
        "//:gpr",
        "//:gpr_base",
        "//src/core/ext/transport/binder/wire_format:transaction",
    ],
)
