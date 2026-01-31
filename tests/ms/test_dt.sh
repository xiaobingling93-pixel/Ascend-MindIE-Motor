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
TEST_LEVEL=${2:-"dt"}
TEST_MODE=${3:-""}

BASE_PATH=$(cd $(dirname $0); pwd)
PROJECT_PATH=$BASE_PATH/../..
PROJECT_PATH=`realpath $PROJECT_PATH`
MINDIE_MS_SRC_PATH=$PROJECT_PATH/mindie_motor/src
MINDIE_MS_TEST_PATH=$PROJECT_PATH/tests/ms
MINDIE_MS_OUTPUT_PATH=$PROJECT_PATH/mindie_motor/src/output

# 检查环境变量 BUILD_MIES_3RDPARTY_INSTALL_DIR 是否为空或未定义
if [ -z "${BUILD_MIES_3RDPARTY_INSTALL_DIR}" ]; then
  # 如果环境变量为空，则设置为当前目录
  BUILD_MIES_3RDPARTY_INSTALL_DIR="$MINDIE_MS_SRC_PATH/3rdparty"
  echo "BUILD_MIES_3RDPARTY_INSTALL_DIR was not set. Set to current directory: $BUILD_MIES_3RDPARTY_INSTALL_DIR"
else
  # 环境变量已存在并且不为空
  echo "BUILD_MIES_3RDPARTY_INSTALL_DIR is already set to: $BUILD_MIES_3RDPARTY_INSTALL_DIR"
fi

source ${MINDIE_MS_TEST_PATH}/ms_test_util.sh

main()
{
    # 替换LOG宏，使其不统计对应分支覆盖率
    python3 ${MINDIE_MS_TEST_PATH}/scripts/replace_log_macro.py
    if [ "$SCRIPT_MODE" != "blue_gate" ]; then
        _build_src ${TEST_MODE}
    fi
    _set_env
    _build_proto

    _build_dt ${TEST_MODE}

    rm -rf ${MINDIE_MS_SRC_PATH}/output
    cp -r ${BUILD_MINDIE_SERVICE_INSTALL_DIR} ${MINDIE_MS_SRC_PATH}
    mv ${MINDIE_MS_SRC_PATH}/install ${MINDIE_MS_SRC_PATH}/output

    chmod 640 -R $MINDIE_MS_SRC_PATH/config/**/*.json
    chmod 640 -R $MINDIE_MS_SRC_PATH/output/config/**/*.json

    _run_smoke_dt_test

    # 运行非门禁测试用例
    _run_other_dt_test

    #生成覆盖率
    _gen_coverage
}

main "$@"
