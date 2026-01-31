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

from om_adapter.common.logging import Log
from om_adapter.config import ConfigUtil
from om_adapter.monitors import select_monitor


logger = Log(__name__).getlog()


class OMAdapter:
    def __init__(self, identity: str):
        self.monitor = select_monitor("CCAE")("mindie", identity)
        logger.info("OM Adapter initialized successfully!")

    def run(self):
        self.monitor.run()


def main():
    if len(sys.argv) <= 1 or (sys.argv[1] != "Controller" and sys.argv[1] != "Coordinator"):
        raise RuntimeError("Need to identify the OM Adapter, available choices are: `Controller` and `Coordinator`.")
    if not ConfigUtil.get_config("ccae"):
        logger.info("Monitor config undetected, om adapter should not initialize!")
        return
    om_adapter = OMAdapter(sys.argv[1])
    om_adapter.run()


if __name__ == '__main__':
    main()
