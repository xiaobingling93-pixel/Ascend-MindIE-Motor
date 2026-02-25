/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MS_CONTROLLERLEADERAGENT_H
#define MS_CONTROLLERLEADERAGENT_H
#include "LeaderAgent.h"
#include "ControllerListener.h"
#include "RequestListener.h"

namespace MINDIE::MS {

class CoordinatorLeaderAgent : public  LeaderAgent {
public:
    CoordinatorLeaderAgent() = default;
    ~CoordinatorLeaderAgent();

    // 单例访问
    static CoordinatorLeaderAgent* GetInstance();

    void Master2Slave() override;
    void Slave2Master() override;
    void Slave2MasterEvent() override;
    void SetListener(ControllerListener* cl, RequestListener* rl);

private:
    ControllerListener* controllerListener = nullptr;
    RequestListener* requestListener = nullptr;
};

} // namespace MINDIE::MS

#endif // MINDIE_MS_CONTROLLERLEADERAGENT_H
