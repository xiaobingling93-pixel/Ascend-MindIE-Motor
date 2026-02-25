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
#include <memory>
#include <map>
#include <functional>
#include "SchedulerFactory.h"

namespace MINDIE::MS {

SchedulerFactory &SchedulerFactory::GetInstance()
{
    static SchedulerFactory instance;
    return instance;
}

std::unique_ptr<DIGSScheduler> SchedulerFactory::CreateScheduler(const std::string &name, DIGSScheduler::Config config)
{
    if (schedulerCreatorList.find(name) != schedulerCreatorList.end()) {
        return schedulerCreatorList[name](config);
    }
    return nullptr;
}

bool SchedulerFactory::RegisterSchedulerCreator(const std::string &name, const SchedulerCreator &creator)
{
    if (schedulerCreatorList.find(name) != schedulerCreatorList.end()) {
        return false;
    }
    schedulerCreatorList[name] = creator;
    return true;
}
}