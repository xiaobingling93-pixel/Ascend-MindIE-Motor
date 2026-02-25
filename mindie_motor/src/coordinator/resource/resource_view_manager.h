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
#ifndef MINDIE_DIGS_RESOURCE_VIEW_MANAGER_H
#define MINDIE_DIGS_RESOURCE_VIEW_MANAGER_H

#include "request/digs_request.h"
#include "resource_info.h"
#include "Logger.h"
#include "common.h"
#include "resource_load.h"


namespace MINDIE::MS {

class ResourceViewManager {
public:
    using ResView = std::vector<std::pair<std::shared_ptr<ResourceInfo>, std::shared_ptr<ResourceLoad>>>;

    using ResInfo = std::pair<std::shared_ptr<ResourceInfo>, std::shared_ptr<ResourceLoad>>;

    explicit ResourceViewManager(size_t maxResNum);

    ~ResourceViewManager() = default;

    const ResView& PrefillPool() { return prefillPool_; }

    int32_t Add2PrefillPool(ResInfo resInfo);

    const std::map<uint64_t, ResView>& DecodePool() { return decodePool_; }

    int32_t Add2DecodePool(ResInfo resInfo, uint64_t groupId);

    const ResView& GlobalPool() { return globalPool_; }

    ptrdiff_t GlobalPoolRemoveAndNext(ptrdiff_t offset, ResInfo& resInfo);

    bool Empty() const;

    void ClearView();

    int32_t AddResInfo(const ResInfo& resInfo);

    int32_t UpdateScheduleInfo(const std::shared_ptr<DIGSRequest>& req, MINDIE::MS::DIGSReqStage stage);

    static ResView& Add2GroupedPool(std::map<uint64_t, ResView>& pool, uint64_t groupId, size_t maxResNum);

    static bool CheckConnection(const ResourceViewManager::ResInfo& resInfo, uint64_t otherResId);

    static bool CheckConnection(const ResourceViewManager::ResInfo& resInfo);

private:
    ResView view_;

    ResView prefillPool_;

    std::map<uint64_t, ResView> decodePool_;

    ResView globalPool_;

    std::map<uint64_t, std::shared_ptr<ResScheduleInfo>> scheduleInfos_;

    size_t maxResNum_ = 5000;
};

}
#endif
