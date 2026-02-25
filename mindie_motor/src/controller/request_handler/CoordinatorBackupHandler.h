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
#ifndef COORDINATORBACKUPHANDLER_H
#define COORDINATORBACKUPHANDLER_H

#include <atomic>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <thread>

#include "NodeStatus.h"
#include "HttpClient.h"
#include "CoordinatorStore.h"
#include "Logger.h"
#include "ControllerConstant.h"
namespace MINDIE::MS {

class CoordinatorBackupHandler {
public:
    explicit CoordinatorBackupHandler(std::shared_ptr<CoordinatorStore> coordinatorStoreInit,
        std::shared_ptr<CoordinatorStore> coordinatorStoreWithMasterInfoInit);
    ~CoordinatorBackupHandler();
    int32_t Init();
    int32_t Run();
    int32_t Start();
    void Stop();
    void DealWithCoordinatorBackup();
    int32_t StartCoordinatorStatusThread();
    int32_t SendCoordinatorBackupRequest(const std::unique_ptr<Coordinator> &node,
        boost::beast::http::verb verb, CoordinatorURI type, std::string& body, std::string &response);

private:
    std::shared_ptr<HttpClient> mCoordinatorClient = nullptr;
    std::shared_ptr<CoordinatorStore> mCoordinatorStore = nullptr;
    std::shared_ptr<CoordinatorStore> mCoordinatorStoreWithMasterInfo = nullptr;
    std::shared_ptr<std::thread> mCoordinatorStatusThread = nullptr;
    std::map<std::string, bool> mCoordinatorBackupStatus = {};
    std::atomic<bool> mRun = { false };
    bool isRandomPick = false;
    void GetCoordinatorBackupInfo(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes);
    void SetCoordinatorIsMasterBackup(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes);
    void UpdateCoordinatorMasterInfo(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes);
};
}
#endif