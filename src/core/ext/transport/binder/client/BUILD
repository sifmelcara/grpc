# TODO(mingcl): describe this package.

load("//bazel:grpc_build_system.bzl", "grpc_cc_library")

licenses(["notice"])

# TODO: Add the flags that warns about undefined symbol at link time
# TOD: See if we can simply use regular cc_library
grpc_cc_library(
    name = "grpc_transport_binder_client",
    srcs = [
        "channel_create.cc",
        "channel_create.h",
        "jni_utils.cc",
    ],
    hdrs = [
        "channel_create.h",
        "jni_utils.h",
    ],
    external_deps = [
        "absl/strings",
        "absl/time",
    ],
    # To be fixed
    visibility = ["//visibility:public"],
    deps = [
        ":grpc_transport_binder_client_impl",
        "//:gpr",
        "//:gpr_base",
        "//:grpc",
        "//:grpc++_base",
        "//:grpc_base",
        "//:grpc_base_c",
        "//:grpc_codegen",
        "//src/core/ext/transport/binder/transport",
        "//src/core/ext/transport/binder/utils:transport_stream_receiver",
        "//src/core/ext/transport/binder/wire_format:binder_android",
        "//src/core/ext/transport/binder/wire_format:server_binder",
        "//src/core/ext/transport/binder/wire_format:wire_writer",
    ],
    alwayslink = 1,
)

grpc_cc_library(
    name = "grpc_transport_binder_client_impl",
    srcs = ["channel_create_impl.cc"],
    hdrs = ["channel_create_impl.h"],
    external_deps = [],
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
        "//src/core/ext/transport/binder/transport",
        "//src/core/ext/transport/binder/wire_format:binder",
    ],
)

# TODO move to elsewhere
cc_library(
    name = "jni_utils",
    srcs = ["jni_utils.cc"],
    hdrs = ["jni_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//:gpr",
    ],
)
