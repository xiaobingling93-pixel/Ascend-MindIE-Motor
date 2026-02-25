/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "Controller.h"
#include "Logger.h"
using namespace MINDIE::MS;
int32_t main()
{
    Controller controller;
    if (controller.Init() != 0) {
        LOG_E("[%s] [Controller] Init controller failed.",
            GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::MAIN).c_str());
        LOG_M("[Exit] Exit because of initializing failure");
        exit(1);
    }
    if (controller.Run() != 0) {
        LOG_E("[%s] [Controller] Run controller failed.",
            GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::MAIN).c_str());
        LOG_M("[Exit] Exit because of running failure.");
        exit(1);
    }
    LOG_M("[Exit] Controller exit.");
    exit(0);
}