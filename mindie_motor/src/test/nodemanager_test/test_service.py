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

import unittest
from unittest.mock import patch, Mock

from node_manager.core import Service, GeneralConfig
from node_manager.core.heartbeat_mng import heartbeat_mng


class TestService(unittest.TestCase):

    def test_get_node_running_status(self):
        running_status = 'normal'
        heartbeat_mng.set_running_status(running_status=running_status)
        self.assertEqual(Service.get_node_running_status(), running_status)

    def test_parse_fault_cmd_info(self):
        fault_cmd_info = {
            "cmd": "PAUSE_ENGINE",
            "extra_info": "info"
        }
        parsed_fault_cmd_info = Service.parse_fault_cmd_info(fault_cmd_info_dict=fault_cmd_info)
        self.assertNotIn(
            'extra_info',
            parsed_fault_cmd_info
        )

    def test_parse_fault_info(self):
        fault_level = "faultLevel"
        hardware_fault_info = {
            "faultNodeInfo": [
                {
                    "faultDeviceInfo": [
                        {
                            "faultCodes": [
                                "[0x00f1fef5,155912,L2,na]",
                                "[0x00f1fef5,155913,na,na]"
                            ],
                            "faultType": ['type0'],
                            "faultReason": ['reason0'],
                            "deviceId": "-1",
                            "deviceType": "Switch",
                            fault_level: "Healthy"
                        },
                        {
                            "faultCodes": [
                                "80C98002",
                                "80CB8002",
                                "80E38003"
                            ],
                            "faultType": ['type1'],
                            "faultReason": ['reason1'],
                            "switchFaultInfos": [],
                            "deviceId": "4",
                            "deviceType": "NPU",
                            fault_level: "UnHealthy"
                        }
                    ],
                    "nodeName": "localhost.localdomain",
                    "nodeIP": "51.38.68.213",
                    "nodeSN": "2102313BFL10LA000023",
                    fault_level: "UnHealthy"
                }
            ],
            "uuid": "2746f3cc-b37f-45a8-8848-84f26ebdf5fd",
            "jobId": "f82c8319-8497-4d1d-8f48-95a25dd9d4c0",
            "signalType": "fault",
        }
        parsed_fault_info = Service.parse_hardware_fault_info(hardware_fault_info_dict=hardware_fault_info)
        self.assertNotIn(
            'switchFaultInfos',
            parsed_fault_info['faultNodeInfo'][0]['faultDeviceInfo'][1]
        )

    @patch('node_manager.core.fault_mng.fault_manager.get_handler')
    def test_fault_handle(self, mock_get_handler: Mock):
        mock_return = {"status": True, "reason": None}
        mock_get_handler.return_value.return_value = mock_return
        cmd = 'PAUSE_ENGINE'
        self.assertEqual(Service.fault_handle(cmd), mock_return)

    @patch("node_manager.daemon_manager.llm_daemon_starter.llm_daemon_manager.terminate_all_processes")
    def test_stop_node_server(self, mock_terminate_all_processes: Mock):
        Service.stop_node_server()
        mock_terminate_all_processes.assert_called_once()

    def test_log_controller_ip(self):
        mock_ip = '192.168.0.1'
        Service.log_controller_ip(ip=mock_ip)
        self.assertEqual(GeneralConfig().controller_ip, mock_ip)


if __name__ == "__main__":
    unittest.main()
