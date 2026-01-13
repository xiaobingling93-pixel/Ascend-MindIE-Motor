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

[ -z "$MINDIE_SERVICE_SRC_DIR" ] && echo "MINDIE_SERVICE_SRC_DIR is not set, exit" && exit 1

BUILD_MIES_3RDPARPT_ROOT_DIR="$MINDIE_SERVICE_SRC_DIR"/third_party

BUILD_MIES_3RDPARPT_ROOT_DIR="$MINDIE_SERVICE_SRC_DIR"/third_party
echo "The BUILD_MIES_3RDPARPT_ROOT_DIR for third party path is: $BUILD_MIES_3RDPARPT_ROOT_DIR"

export THIRD_PARTY_ZIP_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/zipped
export THIRD_PARTY_SRC_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/src

mkdir -p "$THIRD_PARTY_ZIP_DIR"
mkdir -p "$THIRD_PARTY_SRC_DIR"
mkdir -p "$BUILD_MIES_3RDPARPT_ROOT_DIR"

cd "$BUILD_MIES_3RDPARPT_ROOT_DIR"
dependency_json="$MINDIE_SERVICE_SRC_DIR"/ci/dependency.json
python "$MINDIE_SERVICE_SRC_DIR"/ci/download_3rdparty.py "$dependency_json"

unzip_grpc() {
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

# extract
echo "Remove all files under $THIRD_PARTY_SRC_DIR"
rm -rf "$THIRD_PARTY_SRC_DIR"
mkdir -p "$THIRD_PARTY_SRC_DIR"

# prepare grpc is too complicated, so giving a standalone shell script.
echo "unzip grpc..."
unzip_grpc

cd "$THIRD_PARTY_SRC_DIR"

echo "unzip boost..."
tar -zxf "$THIRD_PARTY_ZIP_DIR"/boost.tar.gz
mv boost_1_87_0 boost

echo "unzip openssl..."
tar -zxf "$THIRD_PARTY_ZIP_DIR"/openssl.tar.gz
mv openssl-openssl-3.0.9 openssl

echo "unzip pybind11..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/pybind11.zip
mv pybind11-2.13.6 pybind11

echo "unzip boundscheck..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/boundscheck.zip
mv libboundscheck-v1.1.16 libboundscheck

echo "unzip spdlog..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/spdlog.zip
mv spdlog-1.15.3 spdlog

echo "unzip nlohmann-json..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/nlohmann-json.zip
mv json-3.11.3 nlohmann-json

echo "unzip prometheus-cpp-with-submodules..."
tar -zxf "$THIRD_PARTY_ZIP_DIR"/prometheus-cpp-with-submodules.tar.gz
mv prometheus-cpp-with-submodules prometheus-cpp

echo "unzip google-test..."
tar -zxf "$THIRD_PARTY_ZIP_DIR"/googletest.tar.gz
mv googletest-1.13.0 googletest

echo "unzip cpp_stub..."
tar -zxf "$THIRD_PARTY_ZIP_DIR"/cpp-stub.tar.gz
mv cpp-stub-master cpp-stub

echo "unzip mockcpp..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/mockcpp.zip
mv mockcpp-2.7 mockcpp

echo "unzip makeself..."
unzip -qo "$THIRD_PARTY_ZIP_DIR"/makeself.zip
mv makeself-release-2.5.0 makeself

echo "Success"
