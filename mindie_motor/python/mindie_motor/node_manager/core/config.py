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
import json
import ipaddress
from node_manager.common.utils import _SingletonMeta
from node_manager.common.logging import Log
from ..framework.utils.json_util import JsonUtil

IPV4 = 4
IPV6 = 6
SERVER_CONFIG = "ServerConfig"
MANAGEMENT_PORT = "managementPort"


class GeneralConfig(metaclass=_SingletonMeta):

    def __init__(self):
        self.config_path = None
        self.server_engine_ip = None
        self.server_engine_port_list = None
        self.controller_ip = None  # http_server更新,如果为none则controller未发心跳
        self.controller_port = None
        self.dist_dp_server_enabled = False
        self.has_endpoint = False
        self.server_engine_cnt = 0
        self.http_server_config = {}
        self.heartbeat_interval = 0
        self._update_info()
        self.logger = Log(__name__).getlog()
    
    @staticmethod
    def _check_server_config_valid(config):
        if SERVER_CONFIG not in config:
            raise KeyError(f"Invalid Server Engine Config File")
        server_config = config[SERVER_CONFIG]
        if MANAGEMENT_PORT not in server_config:
            raise KeyError(f"Management Port is Missing in Server Engine Config File")
        if "distDPServerEnabled" not in server_config or not isinstance(server_config["distDPServerEnabled"], bool):
            raise KeyError(f"distDPServerEnabled is Missing or Invalid in Server Engine Config File")

    @staticmethod
    def _load_server_engine_ip():
        pod_ip = os.getenv("POD_IP")
        if pod_ip is None:
            raise KeyError("[Config] Please Check Environment Variable Configuration of POD_IP")
        address = ipaddress.ip_address(pod_ip)
        if address.version != IPV4 and address.version != IPV6:
            raise Exception(f'[Config] POD_IP is not ipv4 or ipv6.')
        return pod_ip

    @classmethod
    def _load_config_path(cls):
        mindie_install_path = os.getenv("MIES_INSTALL_PATH")
        if mindie_install_path is None:
            raise KeyError("[Config] Please Check Environment Variable Configuration of MIES_INSTALL_PATH")
        config_path = os.path.join(mindie_install_path, "conf")
        if not os.path.exists(config_path):
            raise FileNotFoundError(f"[Config] Cannot find Config Directory: {config_path}.")
        return config_path

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

    def _update_info(self):
        self.config_path = self._load_config_path()
        self.http_server_config = JsonUtil.read_json_file(os.path.join(self.config_path, "node_manager.json"))
        self.server_engine_port_list = self._load_server_engine_ports()  # ['10000','10001',...]
        self.server_engine_ip = self._load_server_engine_ip()
        self.server_engine_cnt = self._update_server_engine_cnt()
        self.controller_port = self._load_controller_port()  # str,形如"1000"
        self.heartbeat_interval = self._load_heartbeat_interval()
        self.has_endpoint = self.dist_dp_server_enabled or self._is_master_node()

    def _load_server_engine_ports(self):
        server_engine_port_list = []
        single_server_port = None
        for f_name in sorted(os.listdir(self.config_path)):
            if not f_name.startswith("config") or not f_name.endswith(".json"):
                continue
            _config = JsonUtil.read_json_file(os.path.join(self.config_path, f_name))
            self._check_server_config_valid(_config)
            if f_name == "config.json":
                single_server_port = _config[SERVER_CONFIG][MANAGEMENT_PORT]
            else:
                server_engine_port_list.append(_config[SERVER_CONFIG][MANAGEMENT_PORT])
            self.dist_dp_server_enabled = _config[SERVER_CONFIG]["distDPServerEnabled"]
        if not server_engine_port_list:
            server_engine_port_list.append(single_server_port)
        return server_engine_port_list

    def _is_master_node(self):
        # 判断当前节点是否为master节点  解析hccl.json文件
        ranktable_path = os.getenv("RANK_TABLE_FILE")
        if ranktable_path is None:
            raise KeyError("Please Check Environment Variable Configuration of RANK_TABLE_FILE")
        ranktable = JsonUtil.read_json_file(ranktable_path)
        server_list = ranktable.get("server_list")
        if not isinstance(server_list, list) or not server_list:
            raise KeyError("Invalid ranktable.json file: server_list is missing or invalid")

        return server_list[0].get("container_ip") == self.server_engine_ip

    def _load_controller_port(self):
        if "controller_alarm_port" not in self.http_server_config:
            raise KeyError(
                "[Config] Controller Server Port is Missing in Config File"
            )
        return self.http_server_config["controller_alarm_port"]

    def _load_heartbeat_interval(self):
        interval_key = "heartbeat_interval_seconds"
        if interval_key not in self.http_server_config:
            raise KeyError(
                "[Config] Heartbeat Interval Secends is Missing in Config File"
            )
        if int(self.http_server_config[interval_key]) <= 0:
            raise KeyError(
                f"HeartBeatMng query interval is not set or is invalid, interval={self.query_interval}"
            )
        return int(self.http_server_config[interval_key])

    def _update_server_engine_cnt(self):
        return len(self.server_engine_port_list)
