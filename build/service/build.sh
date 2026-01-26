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

# In default, use gcc 7 to keep compatible with old software.
[ -z "$DT_BLUE" ] && DT_BLUE="true"

[ -z "$GLOBAL_ABI_VERSION" ] && GLOBAL_ABI_VERSION="0"
export GLOBAL_ABI_VERSION
echo "GLOBAL_ABI_VERSION: ${GLOBAL_ABI_VERSION}"

echo "SERVICE_ROOT_DIR: $SERVICE_ROOT_DIR"

MS_DIR="$SERVICE_ROOT_DIR"/mindie_service/management_service
HTTP_CLIENT_CTL_DIR="$SERVICE_ROOT_DIR"/mindie_service/utils/http_client_ctl
THIRD_PARTY_DIR="$SERVICE_ROOT_DIR"/third_party

MS_TEST_DIR="$SERVICE_ROOT_DIR"/tests/ms

# Python wheel

export BUILD_MIES_3RDPARTY_ROOT_DIR="$SERVICE_ROOT_DIR"/third_party
[ -z "$BUILD_MIES_3RDPARTY_INSTALL_DIR" ] && export BUILD_MIES_3RDPARTY_INSTALL_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install
[ -z "$BUILD_MINDIE_SERVICE_INSTALL_DIR" ] && export BUILD_MINDIE_SERVICE_INSTALL_DIR="$SERVICE_ROOT_DIR"/install

# add
export PATH=$BUILD_MIES_3RDPARTY_INSTALL_DIR/grpc/bin:$PATH
export LD_LIBRARY_PATH="$BUILD_MINDIE_SERVICE_INSTALL_DIR"/openssl/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/grpc/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/hseceasy/lib:$BUILD_MIES_3RDPARTY_INSTALL_DIR/prometheus-cpp/lib:$BUILD_MINDIE_SERVICE_INSTALL_DIR/lib:$LD_LIBRARY_PATH

# clean
# llm_backend, ms, mindie_server, http_client_ctl, benchmark, mindieclient
RELEASE=release
if [[ "$DEBUG" == "true" || "$DEBUG" == "1" ]]; then
    RELEASE=debug
fi

function parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
        --release) RELEASE="release"; shift ;;
        --debug) RELEASE="debug"; shift ;;
        *) shift ;;
        esac
    done
}

function build_benchmark() {
    BENCHMARK_SRC_DIR="$SERVICE_ROOT_DIR"/mindie_service/tools/benchmark
    BENCHMARK_DST_DIR="$SERVICE_ROOT_DIR"/install/dist
    mkdir -p "$BENCHMARK_DST_DIR"

    rm -rf ${BENCHMARK_DST_DIR}/mindiebenchmark*

    version_file="${SERVICE_ROOT_DIR}"/version.info
    if [ ! -f "$version_file" ]; then
        echo "Error: MindIE-Service version info file not exits, exit."
        exit
    fi
    version=$(grep "mindie-benchmark" "$version_info_file" | awk -F': *' '{print $2}')
    [ -z "$version" ] && version="1.0.0"  # Using default version if not set
    echo "MindIE benchmark version: $version"
    cd "$BENCHMARK_SRC_DIR"
    cp -f requirements.txt mindiebenchmark/requirements.txt
    python3 setup.py --setup_cmd='bdist_wheel' --version=${version}
    mv dist/mindiebenchmark*.whl "$BENCHMARK_DST_DIR"
    # clean
    rm -rf mindiebenchmark/requirements.txt *.egg-info build/ dist/
    echo "Build mindiebenchmark successfully, saving directory is $BENCHMARK_SRC_DIR"
    cd -
}

function build_client() {
    CLIENT_SRC_DIR="$SERVICE_ROOT_DIR"/mindie_service/client
    CLIENT_DST_DIR="$SERVICE_ROOT_DIR"/install/dist

    rm -rf ${CLIENT_DST_DIR}/mindieclient*
    cd ${CLIENT_SRC_DIR}
    version_file="${SERVICE_ROOT_DIR}"/version.info
    if [ ! -f "$version_file" ]; then
        echo "Error: MindIE-Service version info file not exits, exit."
        exit
    fi
    version=$(grep "mindie-client" "$version_info_file" | awk -F': *' '{print $2}')
    [ -z "$version" ] && version="1.0.0"  # Using default version if not set
    echo "Mindie client version: $version"
    cd "$CLIENT_SRC_DIR"
    python3 setup.py --setup_cmd='bdist_wheel' --version=${version}
    mv dist/mindieclient*.whl "$CLIENT_DST_DIR"
    rm -rf *.egg-info build/ dist/
    echo "Build mindieclient successfully, saving directory is $CLIENT_DST_DIR"
    cd -
}

function download_3rdparty() {
    bash "$SERVICE_ROOT_DIR"/build/third_party/download_and_unzip.sh
}

function build_3rdparty() {
    bash "$SERVICE_ROOT_DIR"/build/third_party/build.sh
}

function build_ms() {
    cd "$MS_DIR"
    if [[ "$RELEASE" == "debug" ]]; then
        bash build.sh -d
    else
        bash build.sh
    fi
    echo "Success"
}

function build_http_client_ctl() {
    cd "$HTTP_CLIENT_CTL_DIR"
    if [[ "$RELEASE" == "debug" ]]; then
        bash build.sh -d
    else
        bash build.sh
    fi
    echo "Success"
}

function fn_extract_debug_symbols() {
    local in_dir=$1
    local out_dir=$2
    mkdir -p $out_dir
    find $in_dir -type f \( -executable -o -name "*.so*" \) | while read -r binary; do
        # Only strip ELF file.
        if file "$binary" | grep -q "ELF"; then
            debug_file="$out_dir/$(basename "$binary").debug"
            objcopy --only-keep-debug "$binary" "$debug_file"
            echo "strip symbols $binary"
            strip --strip-debug --strip-unneeded "$binary"
            objcopy --add-gnu-debuglink="$debug_file" "$binary"
        fi
    done
    echo "Debug symbols have been saved to $out_dir"
}

function build_service() {
    # without third party
    build_ms &
    build_http_client_ctl &
    build_benchmark &
    build_client &
    wait

    if [[ "$RELEASE" == "release" ]]; then
        fn_extract_debug_symbols "$BUILD_MINDIE_SERVICE_INSTALL_DIR" "$BUILD_MINDIE_SERVICE_INSTALL_DIR/motor_debug_symbols"
    fi
}

function build_ms_test() {
    cd "$MS_TEST_DIR"
    if [[ "$DT_BLUE" == "true" ]]; then
        bash test_dt.sh blue_gate dt
    else
        bash test_dt.sh yellow_gate dt
    fi
}

function package_serivce() {
    bash "$SERVICE_ROOT_DIR"/build/service/create_package.sh "$RELEASE"
}

function clean_3rdparty_all() {
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/zipped
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/src
    rm -rf "$BUILD_MIES_3RDPARTY_ROOT_DIR"/build
    echo "Clean 3rdparty success"
}

function clean_service() {
    rm -rf "$MS_DIR"/build
    rm -rf "$HTTP_CLIENT_CTL_DIR"/build
    echo "Clean service success"
}

function main() {
    OPTION=$1
    parse_arguments "$@"
    if [ "$OPTION" == "all" ]; then
        download_3rdparty
        build_3rdparty "$@"
        build_service
        package_serivce
    elif [[ "$OPTION" == "service" || -z "$OPTION" ]]; then
        build_service
        package_serivce
    elif [ "$OPTION" == "clean" ]; then
        clean_service
    elif [ "$OPTION" == "benchmark" ]; then
        build_benchmark
    elif [ "$OPTION" == "client" ]; then
        build_client
    elif [ "$OPTION" == "ms" ]; then
        build_ms
    elif [ "$OPTION" == "http_client_ctl" ]; then
        build_http_client_ctl
    elif [ "$OPTION" == "ms-test" ]; then
        build_ms_test
    elif [ "$OPTION" == "package" ]; then
        package_serivce
    else
        echo "Unknown option: $OPTION"
    fi
}

main $@
