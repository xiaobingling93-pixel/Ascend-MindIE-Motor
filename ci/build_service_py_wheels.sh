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
BUILD_DIR=$(dirname $(readlink -f $0))
PROJ_ROOT_DIR=${BUILD_DIR}/..
[ -z "$BUILD_MINDIE_SERVICE_INSTALL_DIR" ] && export BUILD_MINDIE_SERVICE_INSTALL_DIR="$PROJ_ROOT_DIR/install"

if ! python3 -c "import wheel" 2>/dev/null; then
    echo "Error: Python package 'wheel' is required (same as mindiebenchmark/mindieclient build)."
    echo "Install with: python3 -m pip install --upgrade pip wheel setuptools"
    exit 1
fi

MindIEServiceVersion="${MINDIE_SERVICE_PY_WHEEL_VERSION:-3.0.0}"
if [ ! -f "${PROJ_ROOT_DIR}"/../CI/config/version.ini ]; then
    echo "version.ini is not existed, use default version ${MindIEServiceVersion}"
else
    [ -z "$MINDIE_SERVICE_PY_WHEEL_VERSION" ] && MindIEServiceVersion=$(cat ${PROJ_ROOT_DIR}/../CI/config/version.ini | grep "PackageName" | cut -d "=" -f 2)
fi
MindIEServiceVersion=$(echo $MindIEServiceVersion | sed -E 's/([0-9]+)\.([0-9]+)\.RC([0-9]+)\.([0-9]+)/\1.\2rc\3.post\4/')
MindIEServiceVersion=$(echo $MindIEServiceVersion | sed -s 's!.T!.alpha!')
echo "MindIEServiceVersion (python service wheels) $MindIEServiceVersion"
export MINDIE_SERVICE_PY_WHEEL_VERSION="$MindIEServiceVersion"

DST_DIR="$BUILD_MINDIE_SERVICE_INSTALL_DIR"/dist
mkdir -p "$DST_DIR"
rm -f "$DST_DIR"/om_adapter-*-py3-*.whl "$DST_DIR"/node_manager-*-py3-*.whl

build_one() {
    local name=$1
    cd "$BUILD_DIR/python_wheel_build/$name"
    rm -rf build dist *.egg-info
    python3 setup.py bdist_wheel
    mv dist/${name}*.whl "$DST_DIR"/
    rm -rf build dist *.egg-info
}

build_one om_adapter
build_one node_manager
echo "Built om_adapter and node_manager wheels -> $DST_DIR"
cd -
