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

import json
import time
from json import JSONDecodeError

from om_adapter.share_memory_utils import CircularShareMemory, ByteShareMemory
from om_adapter.common.logging import Log
from .base_backend import BaseBackend


class MindIEBackend(BaseBackend):
    def __init__(self, identity: str):
        super().__init__(identity)
        self.logger = Log(__name__).getlog()
        self.inventory_shm = ByteShareMemory("inventory_sem", "inventory_shm", 10 * 1024 * 1024)
        self.logger.info("Inventory share memory created successfully!")
        if self.identity == "Controller":
            self.alarm_shm = CircularShareMemory("mindie_controller_alarms_sem", "mindie_controller_alarms",
                                                 10 * 1024 * 1024)
            self.logger.info("Alarm share memory created successfully!")
            self.alive_shm = ByteShareMemory(f"smu_ctrl_heartbeat_sem", f"smu_ctrl_heartbeat_shm")
            self.logger.info("Alive share memory created successfully!")
        else:
            self.alarm_shm = CircularShareMemory("mindie_coordinator_alarms_sem", "mindie_coordinator_alarms",
                                                 10 * 1024 * 1024)
            self.logger.info("Alarm share memory created successfully!")
            self.alive_shm = ByteShareMemory(f"smu_coord_heartbeat_sem", f"smu_coord_heartbeat_shm")
            self.logger.info("Alive share memory created successfully!")

    def fetch_alarm_info(self) -> list:
        ans_list = []
        try:
            chunk = self.alarm_shm.read()
            self.logger.debug("Alarm info read from backend is: %s", chunk)
            for elem in chunk.split("\x00"):
                if elem:
                    ans_list.append(elem)
        except Exception as e:
            self.logger.error(e)
        return ans_list

    def fetch_inventory_info(self, model_id: str) -> str:
        try:
            chunk = self.inventory_shm.read()
            self.logger.debug("Inventory info read from backend is: %s", chunk)
            return chunk
        except Exception as e:
            self.logger.error(e)
        return ""

    def is_alive(self) -> bool:
        try:
            chunk = self.alive_shm.read()
            self.logger.debug("Alive info read from backend is: %s", chunk)
            alive_timestamp = json.loads(chunk)["timestamp"]
            if time.time() <= alive_timestamp + 5:
                return True
        except JSONDecodeError as json_error:
            self.logger.error(f"Failed to read timestamp json: {chunk}")
        except Exception as e:
            self.logger.error(e)
        return False
