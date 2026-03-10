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
from time import sleep

sys.path.append(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)
import unittest
from unittest.mock import patch, MagicMock
from unittest.mock import patch, MagicMock
import logging

import requests

from node_manager.models import NodeRunningStatus
from node_manager.core.fault_mng import fault_manager
from node_manager.models.enums import NodeRunningStatus

logging.basicConfig(level=logging.INFO)


class TestFaultManager(unittest.TestCase):
    @classmethod
    def _set_response_data(cls, mocked_request, status_code, mock_data):
        response = MagicMock()
        response.status_code = status_code
        response.json.return_value = mock_data
        mocked_request.return_value = response

    def setUp(self):
        self.fault_manager = fault_manager

    @patch("requests.Session.request")
    def test_send_cmd_to_all_engine(self, mocked_request):
        status_str = "status"
        self.fault_manager.init_heartbeat_mng()

        self.fault_manager.heartbeat_mng.set_running_status(
            NodeRunningStatus.NORMAL.value
        )

        mock_data = {
            status_str: True,
            "message": "",
            "reason": [
                {
                    "device_id": 0,
                    "result": True,
                    "message": None,
                },
            ],
        }
        # 下发命令 pause_engine
        self._set_response_data(mocked_request, 200, mock_data)
        func = self.fault_manager.get_handler("PAUSE_ENGINE")
        response = func()
        logging.info(f"response:{response}")
        self.assertTrue(response[status_str])

        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.PAUSE.value,
        )

        # 下发命令 reinit npu
        self._set_response_data(mocked_request, 200, mock_data)
        ret_true = {status_str: True, "reason": None}
        func = self.fault_manager.get_handler("REINIT_NPU")
        response = func()
        sleep(8)
        self.assertTrue(response[status_str])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.READY.value,
        )

        # 下发命令 start engine
        self._set_response_data(mocked_request, 200, mock_data)
        func = self.fault_manager.get_handler("START_ENGINE")
        response = func()
        logging.info(f"response:{response}")
        self.assertTrue(response[status_str])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.NORMAL.value,
        )

    @patch("requests.Session.request")
    def test_send_false_cmd_to_all_engine(self, mocked_request):
        status_str = "status"
        mock_data = {
            status_str: False,
            "message": "",
            "reason": [
                {
                    "device_id": 0,
                    "result": True,
                    "message": None,
                },
            ],
        }
        # 下发命令 pause_engine
        self._set_response_data(mocked_request, 200, mock_data)
        func = self.fault_manager.get_handler("PAUSE_ENGINE")
        response = func()
        logging.info(f"response:{response}")
        self.assertTrue(not response[status_str])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.ABNORMAL.value,
        )
        # 下发命令 reinit npu
        self._set_response_data(mocked_request, 200, mock_data)
        func = self.fault_manager.get_handler("REINIT_NPU")
        response = func()
        sleep(3)
        logging.info(f"response:{response}")
        self.assertTrue(not response[status_str])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.ABNORMAL.value,
        )

        # 下发命令 start engine
        self._set_response_data(mocked_request, 200, mock_data)
        func = self.fault_manager.get_handler("START_ENGINE")
        response = func()
        logging.info(f"response:{response}")
        self.assertTrue(not response[status_str])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.ABNORMAL.value,
        )

    @patch("requests.Session.request")
    def test_send_request_with_error(self, mocked_request):
        # 验证client发送请求失败的状态

        # request error
        mocked_request.side_effect = requests.exceptions.Timeout()
        response = self.fault_manager.client.get_ep_status(0)
        self.assertFalse(response["success"])

        # process error
        mocked_request.side_effect = KeyError("The specified key does not exist")
        response = self.fault_manager.client.get_ep_status(0)
        self.assertFalse(response["success"])

        # 下发命令 pause_engine
        self._set_response_data(mocked_request, 200, response)
        func = self.fault_manager.get_handler("PAUSE_ENGINE")
        response = func()
        logging.info(f"response:{response}")
        self.assertTrue(not response["status"])
        self.assertEqual(
            self.fault_manager.heartbeat_mng.get_running_status(),
            NodeRunningStatus.ABNORMAL.value,
        )


if __name__ == "__main__":
    unittest.main()
