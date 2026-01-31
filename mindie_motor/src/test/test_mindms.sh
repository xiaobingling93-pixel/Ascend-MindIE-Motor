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

SCRIPT_MODE=${1:-""}
TEST_LEVEL=${2:-"llt"}
TEST_MODE=${3:-""}

BASE_PATH=$(cd $(dirname $0); pwd)
PROJECT_PATH=$BASE_PATH/../..
PROJECT_PATH=`realpath $PROJECT_PATH`
MINDIE_MS_SRC_PATH=$PROJECT_PATH/mindie_motor/src
MINDIE_MS_TEST_PATH=$PROJECT_PATH/mindie_motor/src/test
MINDIE_MS_OUTPUT_PATH=$PROJECT_PATH/mindie_motor/src/output

CPU_TYPE=$(uname -m)
if [[ "${CPU_TYPE}" != "aarch64" && "${CPU_TYPE}" != "x86_64" ]]; then
    echo "It is not system of aarch64 or x86_64"
    exit 1
fi

_download()
{
    if [ ! -d "${MINDIE_MS_TEST_PATH}/open_source" ]; then
        mkdir ${MINDIE_MS_TEST_PATH}/open_source
    fi
    if [ ! -d "${MINDIE_MS_TEST_PATH}/open_source/googletest" ]; then
        cd ${MINDIE_MS_TEST_PATH}/open_source
        wget https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz --no-check-certificate
        tar -zxf v1.13.0.tar.gz
        mv googletest-1.13.0 googletest
        rm -rf v1.13.0.tar.gz
    fi
    if [ ! -d "${MINDIE_MS_TEST_PATH}/open_source/cpp-stub" ]; then
        cd ${MINDIE_MS_TEST_PATH}/open_source
        wget https://github.com/coolxv/cpp-stub/archive/6c0edd20b45439d0455d702c30f79d72bf33b1dd.tar.gz --no-check-certificate
        tar -zxf "6c0edd20b45439d0455d702c30f79d72bf33b1dd.tar.gz"
        mv cpp-stub-6c0edd20b45439d0455d702c30f79d72bf33b1dd cpp-stub
        rm -rf "6c0edd20b45439d0455d702c30f79d72bf33b1dd.tar.gz"
    fi
}

_build_src()
{
    cd $MINDIE_MS_SRC_PATH
    if [ -z "$TEST_MODE" ]; then
        stage="-s $TEST_MODE"
    fi
    sh build.sh -d
}

_build_test()
{
    if [ "$TEST_LEVEL" == 'llt' ]; then
        cd $MINDIE_MS_TEST_PATH/${TEST_LEVEL}
        rm -rf build && mkdir build
        cd build
        cmake -DCMAKE_BUILD_TYPE=Debug ..
        make -j8
    fi
}

pack_test_package()
{
    local package_path=$PROJECT_PATH/test_package
    rm -rf $package_path
    mkdir -p $package_path/MindIE-Service/mindie_motor/src
    cd $package_path
    cp -r  $MINDIE_MS_OUTPUT_PATH $package_path/MindIE-Service/mindie_motor/src/
    cp -r  $PROJECT_PATH/mindie_motor/src/ $package_path/MindIE-Service/
    cp -r  $PROJECT_PATH/tests $package_path/MindIE-Service
    tar cvzf test_package.tar.gz *
}

main()
{
    # 要先执行 sh one_key_build.sh，test对该命令强依赖
    # 下载googletest和cpp-stub
    _download
    # 编译测试代码
    _build_test
    # 最后一步去到相应的测试level运行相应的可执行文件

    # pack_test_package
}

main "$@"