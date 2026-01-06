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

cd "$MODULES_DIR"
if [ ! -d "MindIE-SD" ]; then
    git clone --depth 1 https://gitcode.com/ascend/MindIE-SD.git
else
    echo "MindIE-SD exists in $MODULES_DIR, skip download."
fi
cd "MindIE-SD"
bash build/build.sh
cp output/Ascend-mindie-sd_*_linux.tar.gz "$OUTPUT_DIR"

cd - > /dev/null
