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
#ifndef MINDIE_DIGS_INSTANCE_H
#define MINDIE_DIGS_INSTANCE_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "nlohmann/json.hpp"
#include "digs_instance.h"

namespace MINDIE::MS {
enum class ReqType {
    OPENAI = 0,
    TGI,
    TRITON,
    VLLM,
    MINDIE,
    UNKNOWN
};

class DIGSScheduler {
public:
    using Config = std::map<std::string, std::string>;

    using NotifyPDAllocation = std::function<int32_t(std::string, uint64_t, uint64_t)>;

    using NotifySingleNodeAllocation = std::function<int32_t(std::string, uint64_t)>;

    virtual int32_t RegisterInstance(const std::vector<DIGSInstanceStaticInfo> &instances) = 0;

    virtual int32_t UpdateInstance(const std::vector<DIGSInstanceDynamicInfo> &instances) = 0;

    virtual int32_t RemoveInstance(const std::vector<uint64_t> &instances) = 0;

    virtual int32_t ProcReq(std::string reqId, const std::string &prompt, ReqType type) = 0;
    
    virtual int32_t ProcReq(std::string reqId, size_t promptLen, ReqType type) = 0;

    virtual int32_t ProcReq(__attribute__((unused)) std::string reqId,
        __attribute__((unused)) const std::vector<uint32_t> &tokenList)
    {
        return 0;
    }

    virtual int32_t CloseInstance(__attribute__((unused)) const std::vector<uint64_t> &instances)
    {
        return 0;
    }

    virtual int32_t ActivateInstance(__attribute__((unused)) const std::vector<uint64_t>& instances)
    {
        return 0;
    }

    virtual int32_t Start()
    {
        return 0;
    }

    virtual int32_t Stop()
    {
        return 0;
    }

    virtual int32_t UpdateReq(__attribute__((unused)) std::string reqId, __attribute__((unused)) DIGSReqStage stage,
        __attribute__((unused)) uint64_t prefillEndTime, __attribute__((unused)) uint64_t decodeEndTime,
        __attribute__((unused)) size_t outputLength = 0)
    {
        return 0;
    }

    virtual int32_t RegisterPDNotifyAllocation(__attribute__((unused)) NotifyPDAllocation callback)
    {
        return 0;
    };

     // 注册单机版回调函数
    virtual int32_t RegisterSingleNodeNotifyAllocation(
        __attribute__((unused)) NotifySingleNodeAllocation singlecallback)
    {
        return 0;
    };

    virtual int32_t QueryInstanceScheduleInfo(__attribute__((unused)) std::vector<DIGSInstanceScheduleInfo> &info)
    {
        return 0;
    };

    virtual int32_t QueryRequestSummary(__attribute__((unused)) DIGSRequestSummary &summary)
    {
        return 0;
    }

    virtual void SetBlockSize(size_t blockSize)
    {
        (void)blockSize;
    }

    virtual ~DIGSScheduler() = default;
};
} // namespace MINDIE::MS
#endif
