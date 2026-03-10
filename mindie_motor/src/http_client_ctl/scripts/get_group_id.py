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
import ipaddress

logging.basicConfig(level=logging.INFO)

GLOBAL_RANK_TABLE_ENV = 'GLOBAL_RANK_TABLE_FILE_PATH'


def wait_global_ranktable_completed(argv):
    global_rank_table_path = os.getenv(GLOBAL_RANK_TABLE_ENV)
    if not global_rank_table_path:
        logging.error('read env \"%s\" failed', GLOBAL_RANK_TABLE_ENV)
        return 255
    try:
        while True:
            with open(global_rank_table_path, 'r', encoding='utf-8') as file:
                buf = file.read()
            rank_table = json.loads(buf)
            if rank_table["status"] == "completed":
                break
            else:
                logging.error("status of ranktable is not completed!")
                time.sleep(1)
        server_group_list = rank_table['server_group_list']
        pod_ip = os.getenv('POD_IP')
        if not pod_ip:
            raise RuntimeError("Environment variable 'POD_IP' is not set.")

        try:
            ipaddress.ip_address(pod_ip)
        except ValueError:
            raise RuntimeError(f"Invalid POD_IP: {pod_ip}") from e
        for group in server_group_list:
            group_id = "-1"
            server_list = group["server_list"]
            for server in server_list:
                if server["server_ip"] == pod_ip:
                    group_id = group["group_id"]
            if group_id == '2': # 启动MindIE-Server
                return 2
            elif group_id == '1': # 启动MindIE-MS mindie-ms-controller
                return 1
            elif group_id == '0': # 启动MindIE-MS coordinator
                return 0
            else:
                continue
        return 255
    except Exception as e:
        logging.error(e)
        return 255

if __name__ == "__main__":
    sys.exit(wait_global_ranktable_completed(sys.argv[1:]))