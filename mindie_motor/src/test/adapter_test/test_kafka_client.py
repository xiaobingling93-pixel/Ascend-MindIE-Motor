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

import unittest

from om_adapter.monitors.kafka_client.kafka_produce import KafkaProducer
from om_adapter.config import ConfigUtil


class TestKafkaProduce(unittest.TestCase):
    def test_kafka_config_invalid(self):
        invalid_config_for_param_retries = {
            "bootstrap.servers": "localhost:9091",
            "client.id": "python-producer",
            "acks": "all",
            "retries": "abc",
        }
        self.assertRaises(ValueError, KafkaProducer, invalid_config_for_param_retries)

    def test_kafka_send_message(self):
        valid_config_for_param_retries = {
            "bootstrap.servers": "localhost:9091",
            "client.id": "python-producer",
            'delivery.timeout.ms': 1000  # 1秒超时
        }
        KafkaProducer(valid_config_for_param_retries).send("1", "2")    # 无Raise抛错
