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
import unittest
from unittest.mock import patch
import urllib3

from om_adapter.backends import MindIEBackend
from om_adapter.config import ConfigUtil
from om_adapter.share_memory_utils.abstract_memory import AbstractShareMemoryUtil
from om_adapter.monitors import CCAEMonitor
from om_adapter.common.logging import Log
from . import MockResponse

MODEL_ID = "test_model"


class TestCCAEMonitor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        env_patcher = patch("os.getenv", return_value=MODEL_ID)
        config_patcher = patch.object(ConfigUtil, "config", {
            "monitor_config": {
                "monitor_ip": "127.0.0.1",
                "monitor_port": 1025
            },
            "model_type": "LLM Model"
        })
        sharememory_patcher = patch.object(AbstractShareMemoryUtil, "__init__", return_value=None)
        heartbeat_patcher = patch("om_adapter.backends.mindie_backend.MindIEBackend.is_alive", return_value=True)
        ccae_monitor_patcher = patch.object(CCAEMonitor, "__init__", return_value=None)
        chmod_patcher = patch("os.chmod", return_value=None)
        cls.patchers = [env_patcher, config_patcher, sharememory_patcher, heartbeat_patcher, ccae_monitor_patcher,
                        chmod_patcher]
        for patcher in cls.patchers:
            patcher.start()
        cls.ccae_monitor = CCAEMonitor("mindie", "Controller")
        cls.ccae_monitor.url_prefix = "https://127.0.0.1:1025"
        cls.ccae_monitor.model_id_period = {
            MODEL_ID: [False, 1, 0]
        }
        cls.ccae_monitor.headers = {"Content-Type": "application/json; charset=utf-8"}
        cls.ccae_monitor.backend = MindIEBackend("Controller")
        cls.ccae_monitor.alarm_cache = dict()
        cls.ccae_monitor.component_type = 0
        cls.ccae_monitor.version = "1.0.0"
        cls.ccae_monitor.http_pool_manager = urllib3.PoolManager(cert_reqs="CERT_NONE", timeout=5)
        cls.ccae_monitor.logger = Log(__name__).getlog()

    @classmethod
    def tearDownClass(cls):
        for patcher in cls.patchers:
            patcher.stop()

    def test_send_heart_beat(self):
        mock_heartbeat_response = {
            "reqList": [{
                "modelID": MODEL_ID,
                "inventories": {
                    "forceUpdate": True
                },
                "metrics": {
                    "metricPeriod": 2
                },
                "logsServer": {
                    "topic": "CCAE TOPIC",
                    "servicePort": 31948
                }
            }]
        }
        with patch("urllib3.PoolManager.request", return_value=MockResponse(json.dumps(mock_heartbeat_response))) as p:
            self.ccae_monitor.send_heart_beat()
            self.assertTrue(self.ccae_monitor.model_id_period[MODEL_ID][0])
            self.assertEqual(self.ccae_monitor.model_id_period[MODEL_ID][1], 2)

    def test_upload_new_alarm(self):
        single_alarm_info = [{
            "category": 1,
            "alarmId": "this is a new alarm"
        }]
        with patch.object(AbstractShareMemoryUtil, "read",
                          return_value=json.dumps(single_alarm_info)) as shm_read_p:
            with patch("urllib3.PoolManager.request", return_value=MockResponse("OK")) as response_p:
                self.ccae_monitor.upload_alarm(self.ccae_monitor.backend.fetch_alarm_info()[0])
                self.assertEqual(self.ccae_monitor.alarm_cache["this is a new alarm"], single_alarm_info[0])
                self.assertEqual(self.ccae_monitor.fetch_alarm_cache(), single_alarm_info)

    def test_upload_cancel_alarm(self):
        single_alarm_info = [{
            "category": 2,
            "alarmId": "this is a cancel alarm"
        }]
        with patch.object(AbstractShareMemoryUtil, "read",
                          return_value=json.dumps(single_alarm_info)) as shm_read_p:
            with patch("urllib3.PoolManager.request", return_value=MockResponse("OK")) as response_p:
                self.ccae_monitor.upload_alarm(self.ccae_monitor.backend.fetch_alarm_info()[0])
                self.assertEqual(len(self.ccae_monitor.alarm_cache), 0)

    def test_upload_inventory(self):
        with patch("urllib3.PoolManager.request", return_value=MockResponse("OK")) as p:
            self.assertIsNone(self.ccae_monitor.upload_inventory(str({"inventory": None})))

