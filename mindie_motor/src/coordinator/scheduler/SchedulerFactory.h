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
#ifndef MINDIE_BASE_SCEDULER_FACTORY_H
#define MINDIE_BASE_SCEDULER_FACTORY_H

#include <memory>
#include <map>
#include <functional>
#include "BaseScheduler.h"


namespace MINDIE::MS {

class SchedulerFactory {
public:
    using SchedulerCreator = std::function<std::unique_ptr<DIGSScheduler>(DIGSScheduler::Config config)>;

    static SchedulerFactory &GetInstance();

    // 创建一个调度器，配置为context, 类型为name
    std::unique_ptr<DIGSScheduler> CreateScheduler(const std::string &name, DIGSScheduler::Config config);

    // 注册调度器构造函数
    bool RegisterSchedulerCreator(const std::string &name, const SchedulerCreator &creator);

private:
    SchedulerFactory() = default;
    ~SchedulerFactory() = default;
    // 调度器构造缓存列表
    std::map<std::string, SchedulerCreator> schedulerCreatorList {};
};

#define MINDIE_SCHEDULER_REGISTER_FUNC(name, class, baseClass, func) \
    const static bool schedulerName_##class##_Register = \
        MINDIE::MS::SchedulerFactory::GetInstance().func(name, \
        [](DIGSScheduler::Config config) { \
            auto ptr = std::make_unique<class>(config); \
            return std::unique_ptr<baseClass>(ptr.release()); })

#define MINDIE_SCHEDULER_REGISTER(name, scheduler) \
    MINDIE_SCHEDULER_REGISTER_FUNC(name, scheduler, DIGSScheduler, RegisterSchedulerCreator)
} // namespace MINDIE::MS
#endif // MINDIE_BASE_SCEDULER_FACTORY_H
