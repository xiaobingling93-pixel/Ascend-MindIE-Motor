#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e

# In default, use gcc 7 to keep compatible with old software.
[ -z "$USE_GCC7" ] && export USE_GCC7="false"
[ -z "$DEBUG" ] && DEBUG="false"
[ -z "$DT_BLUE" ] && DT_BLUE="true"
# Check if USE_GCC7 is  1, true or yes
USE_GCC7_LOWER=${USE_GCC7,,}
if [ "$USE_GCC7_LOWER" = "1" ] || [ "$USE_GCC7_LOWER" = "true" ] || [ "$USE_GCC7_LOWER" = "yes" ]; then
    echo "Using GCC7, make sure you use the specific docker image."
    cd /opt/rh/devtoolset-7/root/usr/lib/"$(uname -i)"-linux-gnu
    if [ -f "libstdc++.so.6.0.19" ]; then
        if [ -e "libstdc++.so.6" ]; then
            echo "Warn:libstdc++.so.6 already exists. Please handle it manually."
        else
            ln -sf libstdc++.so.6.0.19 libstdc++.so.6
        fi
    else
        echo "Warn: Not found /opt/rh/devtoolset-7/root/usr/lib/$(uname -i)-linux-gnu/libstdc++.so.6.0.19"
    fi
    export PATH=/opt/rh/devtoolset-7/root/usr/bin:${PATH}
    cd -
fi

[ -z "$GLOBAL_ABI_VERSION" ] && GLOBAL_ABI_VERSION="0"
export GLOBAL_ABI_VERSION
echo "GLOBAL_ABI_VERSION: ${GLOBAL_ABI_VERSION}"

export MINDIE_SERVICE_SRC_DIR=$(realpath $(dirname $0)/..)
echo "MINDIE_SERVICE_SRC_DIR: $MINDIE_SERVICE_SRC_DIR"

SERVER_DIR="$MINDIE_SERVICE_SRC_DIR"/mindie_motor/server
MS_DIR="$MINDIE_SERVICE_SRC_DIR"/mindie_motor/src
LLM_BACKEND_DIR="$MINDIE_SERVICE_SRC_DIR"/MindIE_Backends/MindIE_LLM_Backend
HTTP_CLIENT_CTL_DIR="$MINDIE_SERVICE_SRC_DIR"/mindie_motor/src/http_client_ctl
THIRD_PARTY_DIR="$MINDIE_SERVICE_SRC_DIR"/third_party

SERVER_TEST_DIR="$MINDIE_SERVICE_SRC_DIR"/tests/server
MS_TEST_DIR="$MINDIE_SERVICE_SRC_DIR"/tests/ms

# Python wheel
MINDIE_BENCHMARK_DIR="$MINDIE_SERVICE_SRC_DIR"/mindie_motor/tools/benchmark
MINDIE_CLIENT_DIR="$MINDIE_SERVICE_SRC_DIR"/mindie_motor/client

export BUILD_MIES_3RDPARTY_ROOT_DIR="$MINDIE_SERVICE_SRC_DIR"/third_party
[ -z "$BUILD_MIES_3RDPARTY_INSTALL_DIR" ] && export BUILD_MIES_3RDPARTY_INSTALL_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install
[ -z "$BUILD_MINDIE_SERVICE_INSTALL_DIR" ] && export BUILD_MINDIE_SERVICE_INSTALL_DIR="$MINDIE_SERVICE_SRC_DIR"/install

# add
export PATH=$BUILD_MIES_3RDPARTY_INSTALL_DIR/grpc/bin:$PATH
export LD_LIBRARY_PATH="$BUILD_MINDIE_SERVICE_INSTALL_DIR"/openssl/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/grpc/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/hseceasy/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/prometheus-cpp/lib:$BUILD_MINDIE_SERVICE_INSTALL_DIR/lib:$LD_LIBRARY_PATH

# clean
# llm_backend, ms, mindie_server, http_client_ctl, benchmark, mindieclient

OPTION=$1
shift
ARGS="$@"

replace_script() {
    cp -rf "$MINDIE_SERVICE_SRC_DIR"/overwrite/MindIE-Service/* "$MINDIE_SERVICE_SRC_DIR"
    echo "Success"
}

download_3rdparty() {
    mkdir -p "$BUILD_MIES_3RDPARTY_ROOT_DIR"
    cd "$BUILD_MIES_3RDPARTY_ROOT_DIR"
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/prepare_3rdparty_dependency.sh
}

build_3rdparty() {
    # gRPC need openssl, given the correct path to find correct openssl lib.
    # Make sure build openssl at first before build gRPC.
    export OPENSSL_ROOT_DIR="$BUILD_MINDIE_SERVICE_INSTALL_DIR"/openssl
    export OPENSSL_LIBRARIES="$BUILD_MINDIE_SERVICE_INSTALL_DIR"/openssl/lib
    cd "$BUILD_MIES_3RDPARTY_ROOT_DIR"
    [ ! -d "src" ] && download_3rdparty
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/build_3rdparty.sh "$@"
}

build_3rdparty_for_test() {
    # 3rdparty for test include: `googletest`, `cpp-stub` and `mockcpp`
    cd "$BUILD_MIES_3RDPARTY_ROOT_DIR"
    [ ! -d "src" ] && download_3rdparty
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/build_3rdparty.sh --only-test
}

function generate_grpc_proto ()
{
    local proto_execute="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/protoc"
    local grpc_cpp_plugin_execute="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/grpc_cpp_plugin"
    local proto_install_dir="${MS_DIR}/common/cluster_grpc/grpc_proto"

    local src_file="${proto_install_dir}/recover_mindie.proto"
    local dst_file_one="${proto_install_dir}/recover_mindie.pb.h"
    # If file is not generated, or .proto file is updated, then generate the cxx and header file.
    if [ ! -f "$dst_file_one" ] || [ "$src_file" -nt "$dst_file_one" ]; then
      echo "Generate cxx and header file to grpc_proto from recover_mindie.proto file..."
      # 生成 gRPC 文件
      "${proto_execute}" -I "${proto_install_dir}" --grpc_out="${proto_install_dir}" \
          --plugin=protoc-gen-grpc="${grpc_cpp_plugin_execute}" \
          "${proto_install_dir}/recover_mindie.proto"

      # 生成 C++ 文件
      "${proto_execute}" -I "${proto_install_dir}" --cpp_out="${proto_install_dir}" \
          "${proto_install_dir}/recover_mindie.proto"
    else
      echo "No need to convert. $dst_file_one is up to date."
    fi

    local fault_file="${proto_install_dir}/cluster_fault.proto"
    local dst_fault_file="${proto_install_dir}/cluster_fault.pb.h"
    # If file is not generated, or .proto file is updated, then generate the cxx and header file.
    if [ ! -f "$dst_fault_file" ] || [ "$fault_file" -nt "$dst_fault_file" ]; then
      echo "Generate cxx and header file to grpc_proto from cluster_fault.proto file..."
      # 生成 gRPC 文件
      "${proto_execute}" -I "${proto_install_dir}" --grpc_out="${proto_install_dir}" \
          --plugin=protoc-gen-grpc="${grpc_cpp_plugin_execute}" \
          "${proto_install_dir}/cluster_fault.proto"

      # 生成 C++ 文件
      "${proto_execute}" -I "${proto_install_dir}" --cpp_out="${proto_install_dir}" \
          "${proto_install_dir}/cluster_fault.proto"
    else
      echo "No need to convert. $dst_fault_file is up to date."
    fi
}

build_mies_ms() {
    generate_grpc_proto
    cd "$MS_DIR"
    if [[ "$DEBUG" == "true" ]]; then
        bash build.sh -d
    else
        bash build.sh
    fi
    echo "Success"
}

build_mies_http_client_ctl() {
    cd "$HTTP_CLIENT_CTL_DIR"
    if [[ "$DEBUG" == "true" ]]; then
        bash build.sh -d
    else
        bash build.sh
    fi
    echo "Success"
}

build_mindiebenchmark() {
    cd "$MINDIE_SERVICE_SRC_DIR"
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/build_benchmark.sh
    mkdir -p "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist
    mv dist/mindiebenchmark*.whl "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist
}

build_mindieclient() {
    cd "$MINDIE_SERVICE_SRC_DIR"
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/build_client.sh
    mkdir -p "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist
    mv dist/mindieclient*.whl "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist
}

build_service_python_wheels() {
    cd "$MINDIE_SERVICE_SRC_DIR"
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/build_service_py_wheels.sh
}

function fn_extract_debug_symbols() {
    local in_dir=$1
    local out_dir=$2
    mkdir -p $out_dir
    find $in_dir -type f \( -executable -o -name "*.so*" \) | while read -r binary; do
        debug_file="$out_dir/$(basename "$binary").debug"
        objcopy --only-keep-debug "$binary" "$debug_file"
        echo "strip symbols $binary"
        strip --strip-debug --strip-unneeded "$binary"
        objcopy --add-gnu-debuglink="$debug_file" "$binary"
    done
    echo "Debug symbols have been saved to $out_dir"
}

build_mies() {
    # without third party
    build_mies_ms &
    build_mies_http_client_ctl &
    build_mindiebenchmark &
    build_mindieclient &
    build_service_python_wheels &
    wait
    if [[ "$DEBUG" == "false" ]]; then
        fn_extract_debug_symbols \
            "$BUILD_MINDIE_SERVICE_INSTALL_DIR" "$BUILD_MINDIE_SERVICE_INSTALL_DIR/motor_debug_symbols"
    fi
}

build_ms_test() {
    generate_grpc_proto
    cd "$MS_TEST_DIR"
    if [[ "$DT_BLUE" == "true" ]]; then
        bash test_dt.sh blue_gate dt
    else
        bash test_dt.sh yellow_gate dt
    fi
}

package_mindie_serivce() {
    bash "$MINDIE_SERVICE_SRC_DIR"/ci/create_package.sh "$DEBUG"
}

clean_3rdparty_build() {
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/src
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/build
}

clean_3rdparty_all() {
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/zipped
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/src
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/build
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/install
}

clean_mies_build() {
    rm -rf "$SERVER_DIR"/build
    rm -rf "$MS_DIR"/build
    rm -rf "$LLM_BACKEND_DIR"/build
    rm -rf "$HTTP_CLIENT_CTL_DIR"/build
    rm -rf "$SERVER_DIR"/src/tokenizer/build "$SERVER_DIR"/src/tokenizer/*.egg-info "$SERVER_DIR"/src/tokenizer/dist
    rm -rf "$MINDIE_BENCHMARK_DIR"/build "$MINDIE_BENCHMARK_DIR"/*.egg-info "$MINDIE_BENCHMARK_DIR"/dist
    rm -rf "$MINDIE_CLIENT_DIR"/build "$MINDIE_CLIENT_DIR"/*.egg-info "$MINDIE_CLIENT_DIR"/dist
    rm -f "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist/om_adapter-*-py3-*.whl \
        "$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist/node_manager-*-py3-*.whl
    echo "Clean Success"
}

if [ "$OPTION" == "all" ]; then
    download_3rdparty
    build_3rdparty $ARGS
    build_mies
    package_mindie_serivce
elif [[ "$OPTION" == "mies" || -z "$OPTION" ]]; then
    build_mies
    package_mindie_serivce
elif [ "$OPTION" == "3rdparty-download" ]; then
    download_3rdparty
elif [ "$OPTION" == "3rdparty" ]; then
    build_3rdparty $ARGS
elif [ "$OPTION" == "3rdparty-for-test" ]; then
    build_3rdparty_for_test
elif [ "$OPTION" == "clean" ]; then
    clean_mies_build
elif [ "$OPTION" == "benchmark" ]; then
    build_mindiebenchmark
elif [ "$OPTION" == "client" ]; then
    build_mindieclient
elif [ "$OPTION" == "ms" ]; then
    build_mies_ms
elif [ "$OPTION" == "http_client_ctl" ]; then
    build_mies_http_client_ctl
elif [ "$OPTION" == "ms-test" ]; then
    build_ms_test
elif [ "$OPTION" == "package" ]; then
    package_mindie_serivce
else
    echo "Unknown option: $OPTION"
fi
