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

BENCHMARK_SRC_DIR="$SERVICE_ROOT_DIR"/mindie_motor/tools/benchmark
BENCHMARK_DST_DIR="$SERVICE_ROOT_DIR"/install/dist
mkdir -p "$BENCHMARK_DST_DIR"

rm -rf ${BENCHMARK_DST_DIR}/mindiebenchmark*

version_file="${SERVICE_ROOT_DIR}"/version.info
if [ ! -f "$version_file" ]; then
    echo "Error: MindIE-Service version info file not exits, exit."
    exit
fi
version=$(grep "mindie-benchmark" "$version_info_file" | awk -F': *' '{print $2}')
[ -z "$version" ] && version="$service_version"

echo "MindIE benchmark version: $MindIEServiceVersion"
cd "$BENCHMARK_SRC_DIR"
cp -f requirements.txt mindiebenchmark/requirements.txt
python3 setup.py --setup_cmd='bdist_wheel' --version=${version}
mv dist/mindiebenchmark*.whl "$BENCHMARK_DST_DIR"
# clean
rm -rf mindiebenchmark/requirements.txt *.egg-info build/ dist/
echo "Build mindiebenchmark successfully, saving directory is $BENCHMARK_SRC_DIR"
cd -
