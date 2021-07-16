# TODO(mingcl): describe this package.

load("//bazel:grpc_build_system.bzl", "grpc_cc_library")

licenses(["notice"])

grpc_cc_library(
    name = "transaction",
    srcs = [
    ],
    hdrs = [
        "transaction.h",
    ],
    external_deps = [
        "absl/strings",
    ],
    # TODO fixme
    visibility = ["//visibility:public"],
    deps = [
    ],
)

grpc_cc_library(
    name = "binder",
    srcs = [
    ],
    hdrs = [
        "binder.h",
        "binder_constants.h",
    ],
    external_deps = [
        "absl/strings",
    ],
    # TODO fixme
    visibility = ["//visibility:public"],
    deps = [
        "//:gpr",
    ],
)

grpc_cc_library(
    name = "binder_android",
    srcs = [
        "binder_android.cc",
    ],
    hdrs = [
        "binder_android.h",
    ],
    external_deps = [
        "absl/synchronization",
    ],
    # TODO fixme
    visibility = ["//visibility:public"],
    deps = [
        ":binder",
    ],
)

grpc_cc_library(
    name = "server_binder",
    srcs = ["server_binder.cc"],
    hdrs = ["server_binder.h"],
    external_deps = [
        "absl/strings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":binder",
        ":transaction",
        "//:gpr",
    ],
)

# TODO split interface header
grpc_cc_library(
    name = "wire_writer",
    srcs = ["wire_writer_impl.cc"],
    hdrs = [
        "wire_writer.h",
        "wire_writer_impl.h",
    ],
    external_deps = [
        "absl/container:flat_hash_map",
        "absl/synchronization",
    ],
    # TODO fixme
    visibility = ["//visibility:public"],
    deps = [
        ":binder",
        ":server_binder",
        "//:gpr",
        "//src/core/ext/transport/binder/utils:transport_stream_receiver",
    ],
)
