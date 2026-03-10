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

import os
import json
import logging
import sys
import ast
from file_util import FileUtils

logger = logging.getLogger('my_logger')
logger.setLevel(logging.INFO)

console_handler = logging.StreamHandler(sys.stdout)
formatter = logging.Formatter('%(message)s')
console_handler.setFormatter(formatter)
logger.addHandler(console_handler)


def __check_file_path(file_path, mode, check_permission):
    check_path_flag, err_msg, real_path = FileUtils.regular_file_path(file_path)
    if not check_path_flag:
        logger.error(f"check file path failed: %s", err_msg)
        return False
    check_file_flag, err_msg = FileUtils.is_file_valid(real_path, mode=mode, check_permission=check_permission)
    if not check_file_flag:
        logger.error(f"check file path is invalid: %s", err_msg)
        return False
    return True


def __get_port(json_file_path, key_path, port_type):
    check_permission = os.getenv("MINDIE_CHECK_INPUTFILES_PERMISSION") != "0"
    if not __check_file_path(json_file_path, 0o640, check_permission):
        return -1
    if not os.path.exists(json_file_path):
        return -1

    try:
        with open(json_file_path, 'r', encoding='utf-8') as file:
            config = json.load(file)
    except Exception as e:
        logger.error(f"Failed to load JSON config, {e}")
        return -1

    keys = key_path.split('.')
    current = config
    for key in keys:
        if key not in current:
            logger.error(f"Key not found in config: {key}")
            return -1
        current = current[key]
    port = current
    if port_type == "ip":
        return current
    elif port_type == "str":
        try:
            result = ast.literal_eval(port)
            return result
        except (ValueError, SyntaxError):
            return -1
    if not isinstance(port, int):
        return -1
    if port < 1 or port > 65535:
        return -1
    return port


if __name__ == "__main__":
    json_file_path = sys.argv[1]
    key_path = sys.argv[2]
    port_type = sys.argv[3]

    return_value = __get_port(json_file_path, key_path, port_type)
    logger.info(return_value)