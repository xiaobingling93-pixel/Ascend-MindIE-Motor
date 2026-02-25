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
#ifndef MINDIE_DIGS_PROPORTIONCALCULATOR_H
#define MINDIE_DIGS_PROPORTIONCALCULATOR_H

#include "common.h"
#include "LlamaSimulator.h"
#include "SimulationCalculator.h"
#include "Logger.h"

namespace MINDIE::MS::roleManager {
constexpr uint64_t DIGS_ROLE_FLEX_NUM = 1;
struct ProportionInput {
    uint64_t instanceNum;
    DIGSRequestSummary summary;
    DIGSRatioType type;
    int64_t flexInstNum;
    bool isFirst;
};

class ProportionCalculator {
public:
    explicit ProportionCalculator() : throughput_(0.0), throughputPrev_(0.0)
    {}
    static int32_t Create(std::map<std::string, std::string>& config,
                          std::unique_ptr<ProportionCalculator> &proportionCalculator);

    static int32_t CreateModelSimulator(std::map<std::string, std::string>& config,
                                        std::unique_ptr<ProportionCalculator> &calculator);

    static int32_t CreateLlamaSimulator(std::map<std::string, std::string>& config,
                                 std::unique_ptr<ProportionCalculator> &calculator);

    void CalProportion(size_t instanceNum, DIGSRequestSummary summary,
                       DIGSGroupPDRatio& digsGroupPdRatio);

    int32_t ClusterExpectRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio,
        uint64_t& pRate, uint64_t& dRate) const;
    int32_t CalBestRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio);
    int32_t InitBestRatioWithExternInput(const ProportionInput &input, DIGSGroupPDRatio &ratio);
    void CalSingleInstanceAbility(MINDIE::MS::DIGSRequestSummary summary, double &pInstanceAbility,
        double &dInstanceAbility) const;
private:
    int32_t CalPdRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio) const;
    int32_t CalPdflexRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio);
    int32_t CalPdflexNum(const uint64_t instanceNum, const uint64_t flexNum, DIGSGroupPDRatio &ratio) const;
    int32_t CalFlexPRatioX(DIGSGroupPDRatio &ratio, double &bestThroughput) const;
    void CalPdflexThroughput(DIGSGroupPDRatio ratio, double &tp, double &td) const;
    bool JudgeNeedPdSwtichUseThrput(DIGSGroupPDRatio ratio);
    int32_t SaveRatio(DIGSGroupPDRatio &ratio, DIGSRatioType type);
    int32_t ClusterExpectPdflexRatio(DIGSGroupPDRatio &ratio, uint64_t& pRate, uint64_t& dRate) const;
    int32_t ClusterExpectPdRatio(const DIGSGroupPDRatio &ratio, uint64_t& pRate, uint64_t& dRate) const ;
    int32_t InitPdBestRatioWithExternInput(const ProportionInput &input, DIGSGroupPDRatio &ratio) const;
    int32_t InitPdflexBestRatioWithExternInput(const ProportionInput &input, DIGSGroupPDRatio &ratio) const;

private:
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> modelSimulator_;
    double throughput_;      // 当前PD切换周期的吞吐量
    double throughputPrev_;  // 上个PD切换周期的吞吐量
    DIGSGroupPDRatio ratio_ = {};
    DIGSGroupPDRatio ratioPre_ = {};  // 上个PD切换周期的比例
};

} // digs
#endif
