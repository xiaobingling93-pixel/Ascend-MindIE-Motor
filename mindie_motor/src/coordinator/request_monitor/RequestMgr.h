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
#ifndef MINDIE_MS_REQUEST_MGR_H
#define MINDIE_MS_REQUEST_MGR_H
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <array>
#include <limits>
#include "ServerConnection.h"
#include "BaseScheduler.h"
#include "boost/uuid/uuid_generators.hpp"
#include "HttpClientAsync.h"
#include "ConnectionPool.h"
#include "ClusterNodes.h"
#include "PerfMonitor.h"
#include "ReqAgent.h"
#include "AlarmRequestHandler.h"

namespace MINDIE::MS {
class ReqManage {
public:
    ReqManage(std::unique_ptr<MINDIE::MS::DIGSScheduler>& schedulerInit, std::unique_ptr<PerfMonitor> &perf,
        std::unique_ptr<ClusterNodes>& instancesRecordInit);
    ~ReqManage() = default;
    size_t GetReqNum();
    bool AddReq(boost::beast::string_view reqId, ReqInferType type, std::shared_ptr<ServerConnection> connection,
        boost::beast::http::request<boost::beast::http::dynamic_body> &req);
    std::shared_ptr<ReqAgent> GetReqInfo(boost::beast::string_view reqId);
    std::vector<std::string> GetReqId(std::shared_ptr<ServerConnection> connection);
    void SetRoute(boost::beast::string_view reqId, const std::array<uint64_t, MEMBER_NUM> &route);
    void SetRouteIp(boost::beast::string_view reqId, const std::array<std::string, IP_INFO_MEMBER_NUM> &routeIp);
    std::array<uint64_t, MEMBER_NUM> GetRoute(boost::beast::string_view reqId);
    std::array<std::string, IP_INFO_MEMBER_NUM> GetRouteIp(boost::beast::string_view reqId);
    bool ArePDInstancesValid(boost::beast::string_view reqId, bool skipDecode = false);
    void AddOutputNum(boost::beast::string_view reqId, size_t num);
    size_t GetOutputNum(boost::beast::string_view reqId);
    void UpdateState(boost::beast::string_view reqId, ReqState state);
    static ReqManage& GetInstance();
    std::map<std::string, uint64_t> StatRequest() const;
    const std::vector<uint64_t> GetStateTime(boost::beast::string_view reqId, ReqState state);
    std::map<std::string, std::shared_ptr<ReqAgent>> GetAllReqs();
    bool HasState(boost::beast::string_view reqId, ReqState state);
    void ReleaseFinishedRequest();
    void CheckAndHandleReqCongestionAlarm();
    int32_t GetReqArriveNum();
    void SetReqArriveNum();
    const std::unordered_set<std::string> GetInsRequests(uint64_t insId);

    void SetModelName(boost::beast::string_view reqId, std::string modelName);
    std::string GetModelName(boost::beast::string_view reqId);
    size_t GetReqCount();

private:
    std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler;
    std::unique_ptr<PerfMonitor> &perfMonitor;
    std::unique_ptr<ClusterNodes>& instancesRecord;
    std::shared_mutex reqIdMutex;
    std::map<std::string, std::shared_ptr<ReqAgent>> reqIdMap;
    std::shared_mutex insIdToReqMapMutex;
    std::map<uint64_t, std::unordered_set<std::string>> insIdToReqMap;
    std::atomic<size_t> dRelease;
    std::atomic<size_t> count;
    int64_t recvs;
    int64_t preRecvs;
    std::atomic<size_t> pRecvs;
    std::atomic<size_t> pSends;
    std::atomic<size_t> pRelease;
    std::atomic<size_t> dRecvs;
    std::atomic<bool> inReqCongestionAlarmState = false;
    std::atomic<uint64_t> numAllRequests{0};
    std::atomic<uint64_t> numFailRequests{0};
    std::atomic<uint64_t> numSuccessRequests{0};

    uint64_t GetTimeNow() const;
    void ReqArrive(boost::beast::string_view reqId);
    void ReqScheduled(boost::beast::string_view reqId) const;
    void ReqRepeated(boost::beast::string_view reqId);
    void ReqFinishFirstToken(boost::beast::string_view reqId, uint64_t time);
    void ReqFinish(std::shared_ptr<ReqAgent> reqInfo);
    void ReqRetry(boost::beast::string_view reqId, uint64_t time);
    void ClearReq(boost::beast::string_view reqId, ReqState state, uint64_t time);
};

}
#endif