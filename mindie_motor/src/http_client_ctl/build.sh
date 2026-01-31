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

set -e
readonly CURRENT_PATH=$(cd "$(dirname "$0")"; pwd)
INSTALL_DIR="$BUILD_MINDIE_SERVICE_INSTALL_DIR"
[ -z "$INSTALL_DIR" ] && INSTALL_DIR="$CURRENT_PATH"/output

# Number of processor when execute `make`, if not set, set as `max(1, nproc - 1)`.
[ -z "$NUM_PROC" ] && NUM_PROC=$(( ($(nproc) - 1) > 0 ? $(nproc) - 1 : 1 ))


build_type="Release"
while getopts "d" opt; do
  case ${opt} in
    d)
      build_type="Debug"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

mkdir -p build
cd build

# build http_client_ctl
cmake -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" ..
make install -j${NUM_PROC}
