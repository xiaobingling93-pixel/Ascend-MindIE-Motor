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

rm -rf ${PROJ_ROOT_DIR}/dist/mindieclient*
cd ${PROJ_ROOT_DIR}/mindie_motor/client
MindIEServiceVersion="1.0.0"
if [ ! -f "${PROJ_ROOT_DIR}"/../CI/config/version.ini ]; then
    echo "version.ini is not existed !"
else
    MindIEServiceVersion=$(cat ${PROJ_ROOT_DIR}/../CI/config/version.ini | grep "PackageName" | cut -d "=" -f 2)
fi
MindIEServiceVersion=$(echo $MindIEServiceVersion | sed -E 's/([0-9]+)\.([0-9]+)\.RC([0-9]+)\.([0-9]+)/\1.\2rc\3.post\4/')
MindIEServiceVersion=$(echo $MindIEServiceVersion | sed -s 's!.T!.alpha!')
echo "MindIEServiceVersion $MindIEServiceVersion"
python3 setup.py --setup_cmd='bdist_wheel' --version=${MindIEServiceVersion}
mkdir -p ${PROJ_ROOT_DIR}/dist
mv dist/mindieclient*.whl ${PROJ_ROOT_DIR}/dist
cd -