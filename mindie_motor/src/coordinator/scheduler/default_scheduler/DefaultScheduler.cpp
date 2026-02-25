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
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <functional>
#include "Logger.h"
#include "SchedulerFactory.h"
#include "DefaultScheduler.h"

namespace MINDIE::MS {
DefaultScheduler::~DefaultScheduler()
{
    Stop();
}

// 配置示例：调度算法 key：  Cache亲和，轮询，负载均衡等
// 配置示例：部署形态 key ： 单机 、多机、pd分离
DefaultScheduler::DefaultScheduler(DIGSScheduler::Config config)
{
    if (config["deploy_mode"] == "single_node") {
        deployMode = DeployMode::SINGLE_NODE;
    } else if (config["deploy_mode"] == "pd_separate" ||
               config["deploy_mode"] == "pd_disaggregation" ||
               config["deploy_mode"] == "pd_disaggregation_single_container") {
        deployMode = DeployMode::PD_SEPARATE;
    }

    LOG_I("[DefaultScheduler] Algorithm type is %s.", config["algorithm_type"].c_str());
    if (config["algorithm_type"] == "cache_affinity") {
        algorithmType = AlgorithmMode::CACHE_AFFINITY;
    } else if (config["algorithm_type"] == "load_balance") {
        algorithmType = AlgorithmMode::LOAD_BALANCE;
    } else if (config["algorithm_type"] == "round_robin") {
        algorithmType = AlgorithmMode::ROUND_ROBIN;
    } else {
        LOG_E("[%s] [DefaultScheduler] Configured algorithm type %s is invalid, use default round robin",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::DEFAULT_SCHEDULER).c_str(),
            config["algorithm_type"].c_str());
        algorithmType = AlgorithmMode::ROUND_ROBIN;
    }

    // 创建算法执行器
    nodeStore = std::make_unique<NodeStore>();
    algorithmExecutor = CreateAlgorithmExecutor(algorithmType, nodeStore, config);
}

// 增加某些节点可支持调度
int32_t DefaultScheduler::RegisterInstance(const std::vector<DIGSInstanceStaticInfo> &instances)
{
    LOG_I("[DefaultScheduler] Number of instance registering is %zu.", instances.size());
    return nodeStore->AddNodes(instances);
}

// 删除某些节点
int32_t DefaultScheduler::RemoveInstance(const std::vector<uint64_t> &instances)
{
    return nodeStore->RemoveNodes(instances);
}

// 更新节点动态状态
int32_t DefaultScheduler::UpdateInstance(const std::vector<DIGSInstanceDynamicInfo> &instances)
{
    LOG_D("[DefaultScheduler] Number of instance updating is %zu.", instances.size());
    return nodeStore->DynamicUpdate(instances);
}

// 启动调度器
int32_t DefaultScheduler::Start()
{
    active.store(true);

    // 启动调度线程
    schedulerThread = std::thread([this]() {
        this->SchedThread();
    });

    // 启动转发线程
    repeatThread = std::thread([this]() {
        this->RepeatThread();
    });
    return 0;
}

// 停止调度器
int32_t DefaultScheduler::Stop()
{
    active.store(false);

    schedulerCv.notify_one();
    repeatCv.notify_one();

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
    if (repeatThread.joinable()) {
        repeatThread.join();
    }
    return 0;
}

// 启动调度线程
void DefaultScheduler::SchedThread()
{
    while (active.load()) {
        { // 监听调度信号
            std::unique_lock<std::mutex> lck(schedQueueMtx);
            schedulerCv.wait(lck, [this] {
                if (!waitingScheduleQueue.empty() || !active.load()) {
                    return true;
                } else {
                    return false;
                }
            });
            LOG_D("[DefaultScheduler] Wait schedule waking up.");
        }

        { // 处理调度请求
            while (active.load()) {
                std::unique_lock<std::mutex> lck(schedQueueMtx);
                if (waitingScheduleQueue.empty()) {
                    break;
                }
                auto req = std::move(waitingScheduleQueue.front());
                waitingScheduleQueue.pop();
                lck.unlock();
                // 执行调度请求
                ExecuteSchedRequest(req);
            }
        }
    }
}

// 执行调度请求 填充Cache亲和的算法
void DefaultScheduler::ExecuteSchedRequest(std::unique_ptr<OneRequest> &oneRequest)
{
    // 根据调度算法和部署形态
    std::string reqId = oneRequest->reqId;
    if (algorithmExecutor == nullptr) {
        LOG_E("[%s] [DefaultScheduler] Failed to schedule the request, it will be re-queued. Request ID is %s.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::DEFAULT_SCHEDULER).c_str(),
            reqId.c_str());
        std::unique_lock<std::mutex> lck(schedQueueMtx);
        waitingScheduleQueue.push(std::move(oneRequest));
        return;
        // 调度失败，继续排队
    }
    LOG_D("[DefaultScheduler] Start scheduling request with ID %s.", reqId.c_str());
    std::vector<uint64_t> route = {};
    auto ret = algorithmExecutor->Execute(deployMode, oneRequest->prompt, route, static_cast<int>(oneRequest->type));
    if (ret != 0) {
        LOG_E("[%s] [DefaultScheduler] Failed to schedule the request, it will be re-queued. Request ID is %s.",
              GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::DEFAULT_SCHEDULER).c_str(),
              reqId.c_str());
        std::unique_lock<std::mutex> lck(schedQueueMtx);
        waitingScheduleQueue.push(std::move(oneRequest));
        return;
        // 调度失败，继续排队
    }
    try {
        auto repeatReq = std::make_unique<OneRepeat> ();
        repeatReq->reqId = reqId;
        repeatReq->route = route;
        // 转发结果确定
        {
            // 放入转发器
            std::lock_guard<std::mutex> lck(repeatQueueMtx);
            LOG_D("[DefaultScheduler] Queuing the request for repeat processing. Request ID is %s.",
                  repeatReq->reqId.c_str());
            waitingRepeatQueue.push(std::move(repeatReq));
        }
    } catch (const std::exception& e) {
        LOG_E("[%s] [DefaultScheduler] Execute one request failed, request ID is %s, exception detail is %s.",
            GetErrorCode(ErrorType::EXCEPTION, CoordinatorFeature::DEFAULT_SCHEDULER).c_str(), reqId.c_str(),
            e.what());
    }
    // 唤醒转发器
    repeatCv.notify_one();
    LOG_D("[DefaultScheduler] Finish to schedule request ID %s.", reqId.c_str());
}

// 启动转发线程
void DefaultScheduler::RepeatThread()
{
    while (active.load()) {
        // 监听到信号
        {
            std::unique_lock<std::mutex> lck(repeatQueueMtx);
            repeatCv.wait(lck, [this] {
                if (!waitingRepeatQueue.empty() || !active.load()) {
                    return true;
                } else {
                    return false;
                }
            });
        }
        LOG_D("[DefaultScheduler] Repeat thread signal wake up.");
        {
            // 取出请求
            while (active.load()) {
                std::unique_lock<std::mutex> lck(repeatQueueMtx);
                if (waitingRepeatQueue.empty()) {
                    break;
                }
                auto req = std::move(waitingRepeatQueue.front());
                waitingRepeatQueue.pop();
                lck.unlock();
                // 执行调度请求
                ExecuteRepeatRequest(req);
            }
        }
    }
}

// 执行转发请求
void DefaultScheduler::ExecuteRepeatRequest(std::unique_ptr<OneRepeat> &oneRepeat)
{
    LOG_D("[DefaultScheduler] Start to execute repeat request, deploy mode is %d, request id is %s.", deployMode,
        oneRepeat->reqId.c_str());
    if (deployMode == DeployMode::SINGLE_NODE) {
        singlecallback(oneRepeat->reqId, oneRepeat->route[0]);
    } else if (deployMode == DeployMode::PD_SEPARATE) {
        pdCallback(oneRepeat->reqId, oneRepeat->route[0], oneRepeat->route[1]);
    }
    LOG_D("[DefaultScheduler] Finish to execute repeat request, deploy mode is %d, request id is %s.", deployMode,
        oneRepeat->reqId.c_str());
}

// 放入调度器一个推理请求
int32_t DefaultScheduler::ProcReq(std::string reqId, __attribute__((unused)) const std::vector<uint32_t> &tokenList)
{
    LOG_D("[DefaultScheduler] Processing request %s.", reqId.c_str());
    auto req = std::make_unique<OneRequest>();
    req->reqId = reqId;
    {
        std::lock_guard<std::mutex> guard(schedQueueMtx);
        LOG_D("[DefaultScheduler] Push request into scheduler queue, request ID is %s.", reqId.c_str());
        waitingScheduleQueue.push(std::move(req));
    }
    // 唤醒调度器
    schedulerCv.notify_one();
    return 0;
}

int32_t DefaultScheduler::ProcReq(std::string reqId, const std::string &prompt, MINDIE::MS::ReqType type)
{
    LOG_D("[DefaultScheduler] Processing request %s.", reqId.c_str());
    auto req = std::make_unique<OneRequest>();
    req->reqId = reqId;
    req->prompt = prompt;
    req->type = type;
    {
        std::lock_guard<std::mutex> guard(schedQueueMtx);
        LOG_D("[DefaultScheduler] Push request into scheduler queue, request ID is %s.", reqId.c_str());
        waitingScheduleQueue.push(std::move(req));
    }
    // 唤醒调度器
    schedulerCv.notify_one();
    return 0;
}

int32_t DefaultScheduler::ProcReq(std::string reqId, size_t promptLen, MINDIE::MS::ReqType type)
{
    LOG_E("[DefaultScheduler] Not implemented. Processing request %s. PromptLen: %zu. ReqType: %d",
        reqId.c_str(), promptLen, static_cast<int>(type));
    return -1;
}

// 注册回调函数
int32_t DefaultScheduler::RegisterPDNotifyAllocation(NotifyPDAllocation callback)
{
    pdCallback = callback;
    return 0;
}

int32_t DefaultScheduler::RegisterSingleNodeNotifyAllocation(NotifySingleNodeAllocation callback)
{
    singlecallback = callback;
    return 0;
}

MINDIE_SCHEDULER_REGISTER("default_scheduler", DefaultScheduler);
}