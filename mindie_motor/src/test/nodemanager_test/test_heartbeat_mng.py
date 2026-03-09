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

sys.path.append(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

import threading
import unittest
from time import sleep
import logging
from unittest.mock import patch, MagicMock
from collections import deque

from node_manager.models.enums import NodeRunningStatus, ServiceStatus
from node_manager.core.config import GeneralConfig
from node_manager.core import HeartBeatMng
from node_manager.core.heartbeat_mng import NodeStatusMonitor

logging.basicConfig(level=logging.INFO)
logging.info("This is an info message.")

STATUS_MAP = {
            ServiceStatus.SERVICE_NORMAL.value: [None, NodeRunningStatus.NORMAL.value],
            ServiceStatus.SERVICE_ABNORMAL.value: [None, NodeRunningStatus.ABNORMAL.value],
            ServiceStatus.SERVICE_INIT.value: [None, NodeRunningStatus.INIT.value],
        }
STATUS_STR = "status"
MOCK_RUNNING_DATA = {STATUS_STR: ServiceStatus.SERVICE_NORMAL.value, }


class TestHeartBeatMng(unittest.TestCase):
    _initialized = False

    @classmethod
    def _set_dynamic_return(cls, mocked_request, engine_m, return_map):
        for engine in engine_m:
            mock_data = engine["data"]
            target_url = engine["url"]
            status_code = engine["status_code"]
            response = MagicMock()
            response.status_code = status_code
            response.json.return_value = mock_data
            return_map[target_url] = response

        def dynamic_return(url, **kwargs):
            return return_map.get(url, None)

        mocked_request.side_effect = dynamic_return

    def setUp(self):
        if self._initialized:
            return
        self._running_status = None
        self._lock = threading.Lock()
        self.query_interval = 5
        self.general_config = GeneralConfig()
        self._thread_count = self.general_config.server_engine_cnt
        self.mem_window_len = 1
        self.engine_state = deque(maxlen=self.mem_window_len)
        self._thread_pool = NodeStatusMonitor(self, self.query_interval, engine_state=self.engine_state)
        self._initialized = True
        self.last_hbm_state_ = (None, None)
        self.state_change_map = [0, 1, 2, 1, 2]

        status_code_str = "status_code"
        data_str = "data"
        url_str = "url"

        self.engine_map = [
            {
                data_str: MOCK_RUNNING_DATA,
                status_code_str: 200,
                url_str: "http://127.0.0.1:10000/v1/health/engine/status",
            },
            {
                data_str: MOCK_RUNNING_DATA,
                status_code_str: 200,
                url_str: "http://127.0.0.1:10001/v1/health/engine/status",
            },
            {
                data_str: {data_str: 0},
                status_code_str: 200,
                url_str: "http://127.0.0.1:5000/v1/alarm/llm_engine",
            },
        ]

    def get_running_status(self) -> str:
        return self._running_status

    def set_running_status(self, running_status: str):
        with self._lock:
            if running_status in [status.value for status in NodeRunningStatus]:
                self._running_status = running_status
            else:
                raise ValueError("unknown running status")

    @patch("requests.Session.request")
    def test_get_ep_status_with_thread(self, mocked_request):
        cur_index = 0
        result_map = {}
        self._set_dynamic_return(mocked_request, self.engine_map, result_map)
        running = threading.Event()
        hbm = HeartBeatMng()

        def test_thread():
            hbm.run()

        def hbm_watch():
            while not running.is_set():
                hbm_state_ = None, hbm.get_running_status()
                if hbm_state_ != (None, None) and self.last_hbm_state_ == (None, None):
                    self.last_hbm_state_ = hbm_state_
                if self.last_hbm_state_ != hbm_state_:
                    self.last_hbm_state_ = hbm_state_
                    self.assertEqual(hbm_state_[0],
                                    STATUS_MAP[ServiceStatus(self.state_change_map[cur_index]).value][0])
                    self.assertEqual(hbm_state_[1],
                                    STATUS_MAP[ServiceStatus(self.state_change_map[cur_index]).value][1])
                sleep(1)
        thread = threading.Thread(target=test_thread)
        thread.start()
        thread_2 = threading.Thread(target=hbm_watch)
        thread_2.start()
        while True:
            if cur_index < len(self.state_change_map):
                MOCK_RUNNING_DATA[STATUS_STR], MOCK_RUNNING_DATA[STATUS_STR] = (
                    self.state_change_map[cur_index], self.state_change_map[cur_index]
                )
                self._set_dynamic_return(mocked_request, self.engine_map, result_map)
                sleep(3)
            else:
                break
            cur_index += 1
        running.set()
        hbm.stop()
        sleep(3)

if __name__ == "__main__":
    unittest.main()