#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import logging
import os
import sys
import yaml

from file_util import FileUtils

logger = logging.getLogger('check_logger')
logger.setLevel(logging.INFO)

handler = logging.StreamHandler(sys.stdout)
log_formatter = logging.Formatter('%(message)s')
handler.setFormatter(log_formatter)
logger.addHandler(handler)


def __is_npu_health():
    # 该文件为MA平台特有，如果没有则忽略
    yaml_file = "/opt/cloud/node/npu_status.yaml"
    if (os.path.exists(yaml_file) is False):
        return True

    check_file_flag, _ = FileUtils.is_file_valid(yaml_file)
    if not check_file_flag:
        return False
    check_file_flag = FileUtils.is_symlink(yaml_file)
    if check_file_flag:
        logger.error(f"The path is a symbolic file.")
        return False
    try:
        with open(yaml_file, 'r', encoding='utf-8') as file:
            data = yaml.safe_load(file)
        resources = data.get("resources")
        if resources is None:
            return True
    except Exception:
        # 如果文件非法，则不检查这个文件，认为健康
        return True

    if not isinstance(resources, list):
        return True

    for resource in resources:
        if not isinstance(resource, dict):
            continue
        if ("type" not in resource) or (not resource["type"] == "NPU"):
            continue
        if ("status" in resource):
            for item in resource["status"]:
                if (item["health"] is not None) and (item["health"] is False):
                    return False

    return True

if __name__ == "__main__":
    return_value = __is_npu_health()
    if return_value:
        sys.exit(0)
    else:
        sys.exit(1)
