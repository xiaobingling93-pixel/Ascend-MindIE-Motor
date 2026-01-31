#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import sys
import json
import os
import time
import logging
from file_util import FileUtils
logging.basicConfig(level=logging.INFO)

RANK_TABLE_ENV = 'RANK_TABLE_FILE'
PARSE_ERROR = 255
MASTER_LABEL = 0
SLAVE_LABEL = 1
SERVER_LIST_STR = "server_list"


def get_distribute_role():
    rank_table_path = os.getenv(RANK_TABLE_ENV)
    if not rank_table_path:
        logging.error('read env \"%s\" failed', RANK_TABLE_ENV)
        return PARSE_ERROR
    try:
        check_path_flag, err_msg, real_path = FileUtils.regular_file_path(rank_table_path)
        if not check_path_flag:
            logger.error(f"check file path failed: %s", err_msg)
            return PARSE_ERROR
        with open(real_path, 'r', encoding='utf-8') as file:
            buf = file.read()
        rank_table = json.loads(buf)
        if not rank_table or "status" not in rank_table:
            logging.error("status of ranktable is not exist!")
            return PARSE_ERROR
        if rank_table["status"] != "completed":
            logging.error("status of ranktable is not completed!")
            return PARSE_ERROR
        if (SERVER_LIST_STR not in rank_table or not isinstance(rank_table[SERVER_LIST_STR], list) 
                or len(rank_table[SERVER_LIST_STR]) == 0):
            return PARSE_ERROR
        master_server = rank_table[SERVER_LIST_STR][0]
        pod_ip = os.getenv('MIES_CONTAINER_IP')
        if master_server["container_ip"] == pod_ip:
            return MASTER_LABEL
        else:
            return SLAVE_LABEL
    except Exception as e:
        logging.error(e)
        return PARSE_ERROR

if __name__ == "__main__":
    sys.exit(get_distribute_role())