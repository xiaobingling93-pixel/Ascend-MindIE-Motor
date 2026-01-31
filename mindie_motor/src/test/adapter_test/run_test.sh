#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.


function show_help() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  --subdir <directory>    Specify a subdirectory for tests"
    echo "  --report-path <path>    Specify the path for the report"
    echo "  --help                  Show this help message"
}

function parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --subdir)
                USE_SUB_DIR=True
                TEST_DIR="$2"
                shift 2
                ;;
            --report-path)
                REPORT_PATH="$2"
                shift 2
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

function setup_environment() {
    SCRIPT_DIR=$(dirname "$(realpath "$0")")
    TEST_DIR=$SCRIPT_DIR
    ROOT_TEST_DIR=$(dirname "${SCRIPT_DIR}")
    REPORT_PATH=$ROOT_TEST_DIR/reports
    USE_SUB_DIR=False

    # Get the root home directory
    DT_ROOT_HOME=$(dirname "${ROOT_TEST_DIR}")
    # Define paths
    DT_MIECLIENT_SRC_HOME="$DT_ROOT_HOME/adapter"
    # Set PYTHONPATH
    export PYTHONPATH="$DT_ROOT_HOME:$DT_MIECLIENT_SRC_HOME"
    # Export Required Path for DT
    export DT_ROOT_HOME

    export COVERAGE_FILE="${TEST_DIR}/.coverage"
}

function run_tests() {
    cd $ROOT_TEST_DIR
    python -m venv venv_dt
    source venv_dt/bin/activate
    cd - || exit

    pytest ${TEST_DIR} --cov=om_adapter --cov-branch \
    --junit-xml=${REPORT_PATH}/om_adapter_final.xml --html=${REPORT_PATH}/om_adapter_final.html \
    --self-contained-html \
    --continue-on-collection-errors #避免collecting error中断test session，建议开发环境下取消该参数以提高效率

    coverage html -d ${REPORT_PATH}/report_om_adapter/htmlcov
    coverage xml -o ${REPORT_PATH}/report_om_adapter/coverage.xml
    deactivate
}

function main() {
    setup_environment
    parse_args "$@"
    run_tests
}

main "$@"