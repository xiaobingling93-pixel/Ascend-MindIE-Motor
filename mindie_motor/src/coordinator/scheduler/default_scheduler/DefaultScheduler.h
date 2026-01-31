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
#ifndef MINDIE_DEFAULT_SCEDULER_INSTANCE_H
#define MINDIE_DEFAULT_SCEDULER_INSTANCE_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "BaseScheduler.h"
#include "BaseAlgorithm.h"
#include "NodeStore.h"

namespace MINDIE::MS {
struct OneRequest {
    MINDIE::MS::ReqType type;
    std::string reqId;
    std::string prompt;
};

struct OneRepeat {
    std::string reqId;
    std::vector<uint64_t> route;  // 转发路径
};

class DefaultScheduler : public MINDIE::MS::DIGSScheduler {
public:
    // 配置示例：调度算法 key ： Cache亲和，轮询，负载均衡等
    // 配置示例：部署形态 key ： 单机 、多机、pd分离
    explicit DefaultScheduler(DIGSScheduler::Config config);

    DefaultScheduler(const DefaultScheduler& other) = delete;
    DefaultScheduler& operator=(const DefaultScheduler& other) = delete;
    DefaultScheduler(DefaultScheduler&& other) = delete;
    DefaultScheduler& operator=(DefaultScheduler&& other) = delete;

    // 启动调度器
    int32_t Start() override;
 
    // 停止调度器
    int32_t Stop() override;

     // 增加某些节点可支持调度
    int32_t RegisterInstance(const std::vector<MINDIE::MS::DIGSInstanceStaticInfo> &instances) override;

    // 更新节点动态状态
    int32_t UpdateInstance(const std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> &instances) override;
 
    // 删除某些节点
    int32_t RemoveInstance(const std::vector<uint64_t> &instances) override;

    // 放入调度器一个推理请求
    int32_t ProcReq(std::string reqId, const std::vector<uint32_t> &tokenList) override;
    int32_t ProcReq(std::string reqId, const std::string &prompt, MINDIE::MS::ReqType type) override;
    int32_t ProcReq(std::string reqId, size_t promptLen, MINDIE::MS::ReqType type) override;

    // 注册pd分离回调函数
    int32_t RegisterPDNotifyAllocation(NotifyPDAllocation callback) override;

     // 注册单机版回调函数
    int32_t RegisterSingleNodeNotifyAllocation(NotifySingleNodeAllocation callback) override;

    ~DefaultScheduler() override;
private:
    // 启动调度线程
    void SchedThread();

    // 执行调度请求
    void ExecuteSchedRequest(std::unique_ptr<OneRequest> &oneRequest);

    // 启动转发线程
    void RepeatThread();
 
    // 执行转发请求
    void ExecuteRepeatRequest(std::unique_ptr<OneRepeat> &oneRepeat);

    std::atomic_bool active; // 是否启动

    DeployMode deployMode = DeployMode::PD_SEPARATE; // 部署模式： 单机调度、多机调度、PD分离调度
    AlgorithmMode algorithmType = AlgorithmMode::CACHE_AFFINITY; // 算法类型： Cache亲和，轮询，负载均衡等

    uint32_t maxScheduleCount = 10000; // 最大调度数量
    uint32_t maxrepeatCount = 10000; // 最大转发数量

    std::unique_ptr<NodeStore> nodeStore; // 节点的信息

    std::mutex schedQueueMtx; // 调度队列的锁
    std::queue<std::unique_ptr<OneRequest>> waitingScheduleQueue; // 等待消息调度队列
    std::thread schedulerThread; // 调度线程
    std::condition_variable schedulerCv; // 调度信号，唤醒调度器

    std::mutex repeatQueueMtx; // 消息转发队列的锁
    std::queue<std::unique_ptr<OneRepeat>> waitingRepeatQueue; // 等待消息转发队列
    std::thread repeatThread; // 消息转发线程
    std::condition_variable repeatCv; // 转发信号，唤醒转发器

    NotifyPDAllocation pdCallback; // pd转发请求callback
    NotifySingleNodeAllocation singlecallback; // 单节点转发请求callback

    // 算法执行器， 当前支持Prefix Cache 亲和算法、轮询算法
    std::unique_ptr<BaseAlgorithm> algorithmExecutor;
};
}
#endif