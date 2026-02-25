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

import threading

from node_manager.core.config import GeneralConfig
from node_manager.core.fault_mng import fault_manager
from node_manager.daemon_manager.llm_daemon_starter import llm_daemon_manager
from node_manager.models.dataclass import FaultNodeInfo, FaultDeviceInfo, HardwareFaultInfo, FaultCmd
from node_manager.core.heartbeat_mng import heartbeat_mng


class Service:
    _lock = threading.Lock()
    parse_mode = 'json'

    @classmethod
    def get_node_running_status(cls):
        return heartbeat_mng.get_running_status()

    @classmethod
    def parse_fault_cmd_info(cls, fault_cmd_info_dict: dict):
        fault_cmd_info = FaultCmd(**fault_cmd_info_dict)
        return fault_cmd_info.model_dump(mode=cls.parse_mode)

    @classmethod
    def parse_hardware_fault_info(cls, hardware_fault_info_dict: dict):

        node_info_list = []
        hardware_fault_info = HardwareFaultInfo(**hardware_fault_info_dict)
        for node_info_dict in hardware_fault_info.faultNodeInfo:
            device_info_list = []
            node_info = FaultNodeInfo(**node_info_dict)

            for device_info_dict in node_info.faultDeviceInfo:
                device_info = FaultDeviceInfo(**device_info_dict)

                device_info_list.append(device_info.model_dump(mode=cls.parse_mode))

            node_info.faultDeviceInfo = device_info_list
            node_info_list.append(node_info.model_dump(mode=cls.parse_mode))

        hardware_fault_info.faultNodeInfo = node_info_list
        return hardware_fault_info.model_dump(mode=cls.parse_mode)

    @classmethod
    def fault_handle(cls, cmd: str):
        return fault_manager.get_handler(cmd)()

    @classmethod
    def stop_node_server(cls):
        llm_daemon_manager.terminate_all_processes()

    @classmethod
    def log_controller_ip(cls, ip: str):
        with cls._lock:
            config = GeneralConfig()
            config.controller_ip = ip
