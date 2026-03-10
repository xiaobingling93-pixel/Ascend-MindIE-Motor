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

import os
import sys
import types
from unittest.mock import patch, MagicMock


class FakeGeneralConfig:
    def __init__(self):
        self.config_path = "/fake/path/to/config"
        self.server_engine_ip = "127.0.0.1"
        self.server_engine_port_list = [10000, 10001]
        self.controller_ip = "192.168.0.1"
        self.controller_port = 5000
        self.server_engine_cnt = 2
        self.http_server_config = {}
        self.heartbeat_interval = 5

    @staticmethod
    def has_endpoint():
        return True

    def get_server_engine_port_list(self):
        return self.server_engine_port_list

    def get_server_engine_ip(self):
        return self.server_engine_ip

    def get_server_engine_cnt(self):
        return self.server_engine_cnt

    def get_controller_port(self):
        return self.controller_port

    def get_controller_ip(self):
        return self.controller_ip


fake_module = types.ModuleType("node_manager.core.config")
fake_module.GeneralConfig = FakeGeneralConfig
patcher = patch.dict(sys.modules, {"node_manager.core.config": fake_module})
patcher.start()