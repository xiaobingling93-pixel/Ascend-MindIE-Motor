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

PROJECT_ROOT_DIR=$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")
MODULES_DIR="${PROJECT_ROOT_DIR}"/modules
OUTPUT_DIR="${PROJECT_ROOT_DIR}/output/modules"
mkdir -p "$MODULES_DIR"
mkdir -p "$OUTPUT_DIR"

function copy_with_check()
{
    # Copy only when source file exists and destination file not exist.
    # 'dst' should always be a directory.
    local src=$1
    local dst=$2

    if [[ -d "$src" ]]; then
        cp -rf "$src" "$dst"
        return
    fi

    local dst_file="$dst"/$(basename "$src")
    # Force overwrite if destination exists.
    if [[ -f "$src" ]]; then
        if [ -L "$src" ]; then
            cp -d "$src" "$dst"
        else
            cp -f "$src" "$dst"
        fi
        echo "--- Copy $src to $dst"
    fi
}

function prepare_dependency()
{
    # Prepare dependency for batchscheduler
    [ -z "$BUILD_MIES_3RDPARTY_ROOT_DIR" ] && BUILD_MIES_3RDPARTY_ROOT_DIR="$PROJECT_ROOT_DIR"/third_party
    [ -z "$BUILD_MIES_3RDPARTY_SRC_DIR" ] && BUILD_MIES_3RDPARTY_INSTALL_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install

    local dst_dir="$MODULES_DIR"/MindIE-LLM/lib
    local batchscheduler_dir="$MODULES_DIR"/MindIE-LLM_BatchScheduler/output
    mkdir -p "$dst_dir"

    copy_with_check "$batchscheduler_dir"/libmindie_llm_backend_model.so "$dst_dir"
    copy_with_check "$batchscheduler_dir"/libmindie_llm_ibis.so "$dst_dir"
    copy_with_check "$batchscheduler_dir"/libmindie_llm_ibis.so "$dst_dir"
    copy_with_check "$batchscheduler_dir"/mindie_llm_backend_connector "$dst_dir"
    copy_with_check "$batchscheduler_dir"/model_wrapper-*-py3-*.whl "$dst_dir"

    local third_install_dir="$BUILD_MIES_3RDPARTY_INSTALL_DIR"
    copy_with_check "$third_install_dir"/grpc "$dst_dir"
    copy_with_check "$third_install_dir"/boost/lib/libboost_chrono.so.1.87.0 "$dst_dir"
    copy_with_check "$third_install_dir"/boost/lib/libboost_thread.so.1.87.0 "$dst_dir"
    copy_with_check "$third_install_dir"/libboundscheck/lib/libboundscheck.so "$dst_dir"
    copy_with_check "$third_install_dir"/openssl/libcrypto.so.3 "$dst_dir"
    copy_with_check "$third_install_dir"/openssl/libcrypto.so "$dst_dir"
    copy_with_check "$third_install_dir"/openssl/libssl.so.3 "$dst_dir"
    copy_with_check "$third_install_dir"/openssl/libssl.so "$dst_dir"

    local third_src_dir="${BUILD_MIES_3RDPARTY_ROOT_DIR}"/src
    local dst_dir2="$MODULES_DIR"/MindIE-LLM/third_party
    rm -rf "$dst_dir2"/libboundscheck && cp -rf "$third_src_dir"/libboundscheck "$dst_dir2"
    rm -rf "$dst_dir2"/makeself       && cp -rf "$third_src_dir"/makeself "$dst_dir2"
    rm -rf "$dst_dir2"/nlohmann       && cp -rf "$third_src_dir"/nlohmann-json "$dst_dir2" && mv "$dst_dir2"/nlohmann-json "$dst_dir2"/nlohmann
    rm -rf "$dst_dir2"/spdlog         && cp -rf "$third_src_dir"/spdlog "$dst_dir2"

    # This should be removed if nindie-llm install all dependencies well
    rm -rf "$dst_dir2"/nlohmannJson       && cp -rf "$third_src_dir"/nlohmann-json "$dst_dir2" && mv "$dst_dir2"/nlohmann-json "$dst_dir2"/nlohmannJson
    mkdir -p "$MODULES_DIR"/MindIE-LLM/opensource
    cp -rf "$third_install_dir"/makeself "$MODULES_DIR"/MindIE-LLM/opensource
    cp -f "$MODULES_DIR"/MindIE-LLM/scripts/makeself-header.sh "$MODULES_DIR"/MindIE-LLM/opensource/makeself

}

cd "$MODULES_DIR"
if [ ! -d "MindIE-LLM" ]; then
    git clone --depth 1 https://gitcode.com/ascend/MindIE-LLM.git
else
    echo "MindIE-LLM exists in $MODULES_DIR, skip download."
fi

prepare_dependency

cd "MindIE-LLM"
bash build.sh release --use_cxx11_abi=0
cp output/$(arch)/Ascend-mindie-llm*.run "$OUTPUT_DIR"
echo "Build mindie-llm .run at $OUTPUT_DIR successfully."
