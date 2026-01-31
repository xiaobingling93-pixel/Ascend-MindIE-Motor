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
CLIENT_SRC_DIR="$SERVICE_ROOT_DIR"/mindie_motor/client
CLIENT_DST_DIR="$SERVICE_ROOT_DIR"/install/dist

rm -rf ${CLIENT_DST_DIR}/mindieclient*
cd ${CLIENT_SRC_DIR}
version_file="${SERVICE_ROOT_DIR}"/version.info
if [ ! -f "$version_file" ]; then
    echo "Error: MindIE-Service version info file not exits, exit."
    exit
fi
version=$(grep "mindie-client" "$version_info_file" | awk -F': *' '{print $2}')
[ -z "$version" ] && version="$service_version"
echo "Mindie client version: $version"
cd "$CLIENT_SRC_DIR"
python3 setup.py --setup_cmd='bdist_wheel' --version=${version}
mv dist/mindieclient*.whl "$CLIENT_DST_DIR"
rm -rf *.egg-info build/ dist/
echo "Build mindieclient successfully, saving directory is $CLIENT_DST_DIR"
cd -
