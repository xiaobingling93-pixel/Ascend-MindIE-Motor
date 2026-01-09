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

SERVICE_ROOT_DIR=$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")
CURRENT_DIR=$(dirname "$(realpath "$0")")

BUILD_MIES_3RDPARPT_ROOT_DIR="$SERVICE_ROOT_DIR"/third_party

echo "The BUILD_MIES_3RDPARPT_ROOT_DIR for third party path is: $BUILD_MIES_3RDPARPT_ROOT_DIR"

export THIRD_PARTY_ZIP_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/zipped
export THIRD_PARTY_SRC_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/src

mkdir -p "$THIRD_PARTY_ZIP_DIR"
mkdir -p "$THIRD_PARTY_SRC_DIR"

download() {
    cd "$BUILD_MIES_3RDPARPT_ROOT_DIR"
    python3 "$CURRENT_DIR"/download.py "$CURRENT_DIR"/dependency.json
}

unzip_grpc() {
    echo "unzip grpc..."
    cd "$THIRD_PARTY_SRC_DIR"
    grpc_src_dir="$THIRD_PARTY_SRC_DIR/grpc"    # 放置grpc编译源代码的目录

    mkdir -p "$grpc_src_dir"
    cd "$grpc_src_dir"

    #grpc
    echo "extract grpc"
    rm -rf grpc_source
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/grpc.zip"
    mv grpc-openEuler-24.03-LTS-SP1 grpc_source

    #grpc-zlib
    echo "extract zlib"
    rm -rf zlib
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/zlib.zip"
    mv zlib-openEuler-24.03-LTS-SP1 zlib

    #grpc-protobuf
    echo "extract protobuf"
    rm -rf protobuf
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/protobuf.zip"
    mv protobuf-openEuler-24.03-LTS-SP1 protobuf

    #grpc-abseil-cpp
    echo "extract abseil-cpp"
    rm -rf abseil-cpp
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/abseil-cpp.zip"
    mv abseil-cpp-openEuler-24.03-LTS-SP1 abseil-cpp

    #grpc-re2
    echo "extract re2"
    rm -rf re2
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/re2.zip"
    mv re2-openEuler-24.03-LTS-SP1 re2

    #grpc-cares
    rm -rf cares
    unzip -qo "${THIRD_PARTY_ZIP_DIR}/c-ares.zip"
    mv c-ares-openEuler-24.03-LTS-SP1 cares
}

unzip_boost() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "boost" ]; then
        echo "Found boost source code at '$THIRD_PARTY_SRC_DIR/boost'."
        return
    fi
    echo "unzip boost..."
    tar -zxf "$THIRD_PARTY_ZIP_DIR"/boost.tar.gz
    mv boost_1_87_0 boost
}

unzip_openssl() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "openssl" ]; then
        echo "Found openssl source code at '$THIRD_PARTY_SRC_DIR/openssl'."
        return
    fi
    echo "unzip openssl..."
    tar -zxf "$THIRD_PARTY_ZIP_DIR"/openssl.tar.gz
    mv openssl-openssl-3.0.9 openssl
}

unzip_pybind11() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "pybind11" ]; then
        echo "Found pybind11 source code at '$THIRD_PARTY_SRC_DIR/pybind11'."
        return
    fi
    echo "unzip pybind11..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/pybind11.zip
    mv pybind11-2.13.6 pybind11
}

unzip_libboundscheck() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "libboundscheck" ]; then
        echo "Found libboundscheck source code at '$THIRD_PARTY_SRC_DIR/libboundscheck'."
        return
    fi
    echo "unzip boundscheck..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/boundscheck.zip
    mv libboundscheck-v1.1.16 libboundscheck
}

unzip_spdlog() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "spdlog" ]; then
        echo "Found spdlog source code at '$THIRD_PARTY_SRC_DIR/spdlog'."
        return
    fi
    echo "unzip spdlog..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/spdlog.zip
    mv spdlog-1.15.3 spdlog
}

unzip_nlohmann_json() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "nlohmann-json" ]; then
        echo "Found nlohmann-json source code at '$THIRD_PARTY_SRC_DIR/nlohmann-json'."
        return
    fi
    echo "unzip nlohmann-json..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/nlohmann-json.zip
    mv json-3.11.3 nlohmann-json
}

unzip_prometheus_cpp() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "prometheus-cpp" ]; then
        echo "Found prometheus-cpp source code at '$THIRD_PARTY_SRC_DIR/prometheus-cpp'."
        return
    fi
    echo "unzip prometheus-cpp..."
    tar -zxf "$THIRD_PARTY_ZIP_DIR"/prometheus-cpp-with-submodules.tar.gz
    mv prometheus-cpp-with-submodules prometheus-cpp
}

unzip_google_test() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "googletest" ]; then
        echo "Found googletest source code at '$THIRD_PARTY_SRC_DIR/googletest'."
        return
    fi
    echo "unzip google-test..."
    tar -zxf "$THIRD_PARTY_ZIP_DIR"/googletest.tar.gz
    mv googletest-1.13.0 googletest
}

unzip_cpp_stub() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "cpp-stub" ]; then
        echo "Found cpp-stub source code at '$THIRD_PARTY_SRC_DIR/cpp-stub'."
        return
    fi
    echo "unzip cpp_stub..."
    tar -zxf "$THIRD_PARTY_ZIP_DIR"/cpp-stub.tar.gz
    mv cpp-stub-master cpp-stub
}

unzip_mockcpp() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "mockcpp" ]; then
        echo "Found mockcpp source code at '$THIRD_PARTY_SRC_DIR/mockcpp'."
        return
    fi
    echo "unzip mockcpp..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/mockcpp.zip
    mv mockcpp-2.7 mockcpp
}

unzip_makeself() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "makeself" ]; then
        echo "Found makeself source code at '$THIRD_PARTY_SRC_DIR/makeself'."
        return
    fi
    echo "unzip makeself..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/makeself.zip
    mv makeself-release-2.5.0 makeself
}

unzip_protobuf() {
    cd "$THIRD_PARTY_SRC_DIR"
    if [ -d "protobuf" ]; then
        echo "Found protobuf source code at '$THIRD_PARTY_SRC_DIR/protobuf'."
        return
    fi
    echo "unzip protobuf-3.13.0..."
    unzip -qo "$THIRD_PARTY_ZIP_DIR"/protobuf-3.13.0.zip
    mv protobuf-3.13.0 protobuf
}

main()
{
    # download
    download
    # extract
    echo "Extract compressed package, if the destination directory exists, skip to extract. " \
        "You can remove the destination directory manually if you want to re-uncompress it."
    unzip_grpc
    unzip_boost
    unzip_openssl
    unzip_pybind11
    unzip_libboundscheck
    unzip_spdlog
    unzip_nlohmann_json
    unzip_prometheus_cpp
    unzip_google_test
    unzip_cpp_stub
    unzip_mockcpp
    unzip_makeself
    unzip_protobuf

    echo "Success"
}

main
