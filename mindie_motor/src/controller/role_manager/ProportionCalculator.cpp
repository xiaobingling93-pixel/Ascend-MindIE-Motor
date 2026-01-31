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
#include "ProportionCalculator.h"
#include <securec.h>

using namespace MINDIE::MS;
namespace MINDIE::MS::roleManager {

constexpr size_t CRUISES_SPACE = 16;
constexpr size_t MAX_INSTANCE_NUM = 16;
constexpr double FLEX_CHANGE_LOSS_ALPHA = 1.0;  // FLEX实例中P/D配比变化性能损耗
constexpr double FLEX_P_OR_D_PERCENTAGE = 0.5;  // 组内P/D实例的比例至少为0.5
constexpr double FLEX_P_PERCENTAGE_MAX = 1.0;
constexpr double FLEX_X_DOWN_THR = 0.0;                                                // x的下限
constexpr double FLEX_X_UP_THR = 1.0;                                                  // x的上限
constexpr double STEP_LENGTH = 10.0;                                                  // 步长
constexpr double INIT_STEP_LENGTH = ((FLEX_X_UP_THR - FLEX_X_DOWN_THR) / STEP_LENGTH);  // 初始步长
constexpr double CONVERGENCE_THR = 1e-8;                                              // 收敛阈值
constexpr int MAX_ITERATION_NUM = 1000;                                               // 最大迭代次数
constexpr double P_THREOUGHPUT = 0.0;
constexpr double D_THREOUGHPUT = 0.0;
constexpr double DESCREASE_STEP_SZIE = 0.99;

int32_t ProportionCalculator::Create(std::map<std::string, std::string> &config,
                                     std::unique_ptr<ProportionCalculator> &proportionCalculator)
{
    try {
        auto calculator = std::make_unique<ProportionCalculator>();
        if (CreateModelSimulator(config, calculator) != static_cast<int32_t>(common::Status::OK)) {
            LOG_E("[%s] [DIGS] Create model simulator error.",
                  GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
        if (simulation::SimulationCalculator::GetInstance().Create(config) !=
            static_cast<int32_t>(common::Status::OK)) {
            LOG_E("[%s] [RoleManager] Create Simulation Calculator failed",
                  GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
        proportionCalculator = std::move(calculator);
    } catch (const std::exception &e) {
        LOG_E("[%s] [StatusUpdater] Failed to create proportion calculator. Error: %s.",
              GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, CommonFeature::DIGS).c_str(), e.what());
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CreateModelSimulator(std::map<std::string, std::string>& config,
                                                   std::unique_ptr<ProportionCalculator> &calculator)
{
    std::string modelType;
    do {
        if (!MINDIE::MS::common::GetConfig("model_type", modelType, config)) {
            if (CreateLlamaSimulator(config, calculator) != static_cast<int32_t>(common::Status::OK)) {
                return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
            }
            break;
        }

        std::transform(modelType.begin(), modelType.end(), modelType.begin(), ::tolower);
        if (modelType.find("llama") != std::string::npos) {
            if (CreateLlamaSimulator(config, calculator) != static_cast<int32_t>(common::Status::OK)) {
                return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
            }
            break;
        }

        LOG_W("[%s] [DIGS] Cannot find model type, create by default model(Llama) type.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        if (CreateLlamaSimulator(config, calculator) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
    } while (false);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CreateLlamaSimulator(std::map<std::string, std::string>& config,
                                                   std::unique_ptr<ProportionCalculator> &calculator)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    if (MINDIE::MS::simulation::LlamaSimulator::Create(config, simulator) != static_cast<int32_t>(common::Status::OK)) {
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (calculator == nullptr) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    calculator->modelSimulator_ = std::move(simulator);
    return static_cast<int32_t>(common::Status::OK);
}

void ProportionCalculator::CalProportion(size_t instanceNum, MINDIE::MS::DIGSRequestSummary summary,
                                         DIGSGroupPDRatio &digsGroupPdRatio)
{
    modelSimulator_->CalAbility(summary, digsGroupPdRatio);
    auto singleDecodeAbility = digsGroupPdRatio.dAbility;
    auto singlePrefillAbility = digsGroupPdRatio.pAbility;

    size_t pInstance = 0;
    size_t dInstance = 0;

    if (singlePrefillAbility <= 0 || singleDecodeAbility <= 0) {
        digsGroupPdRatio.dNum = instanceNum >> 1;
        digsGroupPdRatio.pNum = instanceNum - digsGroupPdRatio.dNum;
        LOG_W("[%s] [DIGS] Invalid ability values.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        return;
    }

    while (pInstance + dInstance < instanceNum) {
        double pRate = singlePrefillAbility * pInstance;
        double dRate = singleDecodeAbility * dInstance;

        if (pRate <= dRate) {
            pInstance++;
        } else {
            dInstance++;
        }
    }

    digsGroupPdRatio.dNum = dInstance;
    digsGroupPdRatio.pNum = pInstance;
    digsGroupPdRatio.pdRatio = singleDecodeAbility / singlePrefillAbility;
}

int32_t ProportionCalculator::CalBestRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio)
{
    int32_t ret;
    simulation::SimulationCalculator::GetInstance().CalAbility(input.summary, ratio);
    if (MINDIE::MS::common::DoubleIsZero(ratio.pAbility) || common::DoubleIsZero(ratio.dAbility) ||
        MINDIE::MS::common::DoubleIsZero(ratio.tAbility)) {
        LOG_W("[RoleManager] Ability is zero!");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (MINDIE::MS::common::DoubleLess(ratio.pAbility, 0) || MINDIE::MS::common::DoubleLess(ratio.dAbility, 0) ||
        MINDIE::MS::common::DoubleLess(ratio.tAbility, 0)) {
        LOG_W("[RoleManager] Ability is less than zero");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (input.type == DIGSRatioType::DIGS_ROLE_PD_RATIO) {
        ret = CalPdRatio(input, ratio);
    } else if (input.type == DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO) {
        ret = CalPdflexRatio(input, ratio);
    } else {
        LOG_E("[RoleManager] invalide ratio type");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }

    if (ret != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    if (SaveRatio(ratio, input.type) != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CalPdRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio) const
{
    modelSimulator_->CalAbility(input.summary, ratio);
    auto singleDecodeAbility = ratio.dAbility;
    auto singlePrefillAbility = ratio.pAbility;
    size_t pInstance = 0;
    size_t dInstance = 0;
    if (singlePrefillAbility <= 0 || singleDecodeAbility <= 0) {
        ratio.dNum = input.instanceNum >> 1;
        ratio.pNum = input.instanceNum - ratio.dNum;
        LOG_W("[%s] [DIGS] Invalid ability values.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    while (pInstance + dInstance < input.instanceNum) {
        double pRate = singlePrefillAbility * pInstance;
        double dRate = singleDecodeAbility * dInstance;

        if (pRate <= dRate) {
            pInstance++;
        } else {
            dInstance++;
        }
    }
    ratio.dNum = dInstance;
    ratio.pNum = pInstance;
    ratio.pdRatio = singleDecodeAbility / singlePrefillAbility;
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CalPdflexRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio)
{
    // Proportion模块是有状态的。计算最佳实例配比需要依赖上一周期的配比信息。
    bool needPdSwitch = true;
    int32_t ret = CalPdflexNum(input.instanceNum, input.flexInstNum, ratio);
    LOG_I("pNum " << ratio.pNum << " dNum " << ratio.dNum << " flexNum " << ratio.flexNum << " flexPRatio "
                          << ratio.flexPRatio << " pAbility " << ratio.pAbility << " dAbility " << ratio.dAbility
                          << " tAbility " << ratio.tAbility << " ret " << ret);
    if (ret != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    if (!input.isFirst) {  //
        // 1、考虑到PD切换的时间损耗：扣除进行切换的实例，重新计算T_p和T_d
        // 如果仍然满足T_p>T_d（P->D）或者T_d>T_p（D->P），则可以做PD身份切换
        if (!JudgeNeedPdSwtichUseThrput(ratio)) {
            needPdSwitch = false;
        }
        // 不需要PD切换，进行Flex比例微调
        if (!needPdSwitch) {
            ratio.pNum = ratioPre_.pNum;
            ratio.dNum = ratioPre_.dNum;
            double t;
            CalFlexPRatioX(ratio, t);
        }
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CalFlexPRatioX(DIGSGroupPDRatio &ratio, double &bestThroughput) const
{
    auto pAbility = ratio.pAbility;
    auto dAbility = ratio.dAbility;
    auto tAbility = ratio.tAbility;
    auto pNum = ratio.pNum;
    auto dNum = ratio.dNum;
    auto x = ratio.flexPRatio;
    if (common::DoubleIsZero(ratio.pAbility) || common::DoubleIsZero(ratio.dAbility) ||
        common::DoubleIsZero(ratio.tAbility)) {
        LOG_W("[RoleManager] Ability is zero!");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (common::DoubleLess(ratio.pAbility, 0) || common::DoubleLess(ratio.dAbility, 0) ||
        common::DoubleLess(ratio.tAbility, 0)) {
        LOG_W("[RoleManager] Ability is less than zero");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    double lb = FLEX_X_DOWN_THR;          // x的下限
    double ub = FLEX_X_UP_THR;            // x的上限
    double stepSize = INIT_STEP_LENGTH;  // 初始步长
    double tol = CONVERGENCE_THR;        // 收敛阈值
    int iterations = MAX_ITERATION_NUM;  // 最大迭代次数
    double pThrouthput = P_THREOUGHPUT;
    double dThrouthput = D_THREOUGHPUT;
    // 迭代优化
    for (int iter = 0; iter < iterations; ++iter) {
        // 计算 P 和 D 的吞吐量
        pThrouthput = pNum * std::min(pAbility, tAbility) + FLEX_CHANGE_LOSS_ALPHA * std::min(x * pAbility, tAbility);
        dThrouthput = dNum * std::min(dAbility, tAbility) +
                      FLEX_CHANGE_LOSS_ALPHA * std::min((FLEX_P_PERCENTAGE_MAX - x) * dAbility, tAbility);
        // 计算误差，如果已足够小则认为收敛
        if (std::abs(pThrouthput - dThrouthput) < tol) {
            break;
        }
        // 评估当前解的吞吐量并根据吞吐量调整 x
        if (pThrouthput > dThrouthput) {
            x -= stepSize;  // P 吞吐量大，减小 x
        } else {
            x += stepSize;  // D 吞吐量大，增大 x
        }
        // 确保 x 在 [lb, ub] 范围内
        x = std::max(lb, std::min(x, ub));
        // 动态调整步长：随着迭代次数的增加逐渐减小步长
        stepSize *= DESCREASE_STEP_SZIE;  // 每次减少步长
    }

    // 返回最佳结果
    bestThroughput = std::min(pThrouthput, dThrouthput);  // 选择最优吞吐量
    ratio.flexPRatio = x;                                  // 返回最佳 x
    if (pNum == 0 && common::DoubleLess(ratio.flexPRatio, FLEX_P_OR_D_PERCENTAGE)) {
        ratio.flexPRatio = FLEX_P_OR_D_PERCENTAGE;
    }
    if (dNum == 0 && common::DoubleGreater(ratio.flexPRatio, FLEX_P_OR_D_PERCENTAGE)) {
        ratio.flexPRatio = FLEX_P_OR_D_PERCENTAGE;
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::CalPdflexNum(const uint64_t instanceNum, const uint64_t flexNum,
                                           DIGSGroupPDRatio &ratio) const
{
    auto pAbility = ratio.pAbility;
    auto dAbility = ratio.dAbility;
    auto tAbility = ratio.tAbility;
    ratio.flexNum = flexNum;
    double pOutput = std::min(pAbility, tAbility);
    double dOutput = std::min(dAbility, tAbility);
    double sumAbility = pOutput + dOutput;
    if (common::DoubleIsZero(ratio.pAbility) || common::DoubleIsZero(ratio.dAbility) ||
        common::DoubleIsZero(ratio.tAbility)) {
        LOG_W("[RoleManager] Ability is zero!");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (common::DoubleLess(ratio.pAbility, 0) || common::DoubleLess(ratio.dAbility, 0) ||
        common::DoubleLess(ratio.tAbility, 0)) {
        LOG_W("[RoleManager] Ability is less than zero");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    // 向下取整 P 实例个数，剩余的分配给 D 实例
    DIGSGroupPDRatio ratioDown = ratio;
    ratioDown.pNum = static_cast<uint64_t>(std::floor(instanceNum * dOutput / (sumAbility)));
    double tDown = 0;
    if (ratioDown.pNum + flexNum <= instanceNum) {
        ratioDown.dNum = instanceNum - ratioDown.pNum - flexNum;
        CalFlexPRatioX(ratioDown, tDown);
    } else {
        ratioDown.dNum = 0;
        ratioDown.flexPRatio = 0;
    }

    // 向上取整 P 实例个数, 剩余的分配给 D 实例
    DIGSGroupPDRatio ratioUp = ratio;
    ratioUp.pNum = static_cast<uint64_t>(std::ceil(instanceNum * dOutput / (sumAbility)));
    double tUp = 0;
    if (ratioUp.pNum + flexNum <= instanceNum) {
        ratioUp.dNum = instanceNum - ratioUp.pNum - flexNum;
        CalFlexPRatioX(ratioUp, tUp);
    } else {
        ratioUp.dNum = 0;
        ratioUp.flexPRatio = 0;
    }
    // 选择吞吐量较大的配置
    if (tDown > tUp) {
        ratio = ratioDown;
    } else {
        ratio = ratioUp;
    }
    return static_cast<int32_t>(common::Status::OK);
}

void ProportionCalculator::CalPdflexThroughput(DIGSGroupPDRatio ratio, double &tp, double &td) const
{
    auto pAbility = common::MinDouble(ratio.pAbility, ratio.tAbility);
    auto dAbility = common::MinDouble(ratio.dAbility, ratio.tAbility);
    uint64_t flexNum = ratio.flexNum;
    double pRatioX = ratio.flexPRatio;
    double dRatio = 1.0 - pRatioX;
    tp = double(pAbility * ratio.pNum) + double(FLEX_CHANGE_LOSS_ALPHA * flexNum * pAbility * pRatioX);
    td = double(dAbility * ratio.dNum) + double(FLEX_CHANGE_LOSS_ALPHA * flexNum * dAbility * dRatio);
}

bool ProportionCalculator::JudgeNeedPdSwtichUseThrput(DIGSGroupPDRatio ratio)
{
    DIGSGroupPDRatio ratioTmp;
    double tp;
    double td;
    if (memcpy_s(&ratioTmp, sizeof(DIGSGroupPDRatio), &ratio, sizeof(DIGSGroupPDRatio)) != EOF) {
        return true;
    }
    if (ratio.pNum < ratioPre_.pNum) {
        // P实例数减少，表示做P->D切换。扣除切换实例，如果T_p > T_d，则可以做PD切换
        ratioTmp.dNum = ratioPre_.dNum;
        CalPdflexThroughput(ratioTmp, tp, td);
        LOG_I("P->D: tp " << tp << " td " << td);
        if (common::DoubleGreater(tp, td)) {
            return true;
        }
    } else if (ratio.dNum < ratioPre_.dNum) {
        // D实例数减少，表示做D->P切换。扣除切换实例，如果T_d > T_p，则可以做PD切换
        ratioTmp.pNum = ratioPre_.pNum;
        CalPdflexThroughput(ratioTmp, tp, td);
        LOG_I("D->P: tp " << tp << " td " << td);
        if (common::DoubleGreater(td, tp)) {
            return true;
        }
    }
    return false;
}

int32_t ProportionCalculator::ClusterExpectRatio(const ProportionInput &input, DIGSGroupPDRatio &ratio, uint64_t &pRate,
                                                 uint64_t &dRate) const
{
    int32_t ret;
    simulation::SimulationCalculator::GetInstance().CalAbility(input.summary, ratio);
    if (common::DoubleIsZero(ratio.pAbility) || common::DoubleIsZero(ratio.dAbility) ||
        common::DoubleIsZero(ratio.tAbility)) {
        LOG_W("[RoleManager] Ability is zero!");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (common::DoubleLess(ratio.pAbility, 0) || common::DoubleLess(ratio.dAbility, 0) ||
        common::DoubleLess(ratio.tAbility, 0)) {
        LOG_W("[RoleManager] Ability is less than zero");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (input.type == DIGSRatioType::DIGS_ROLE_PD_RATIO) {
        ret = ClusterExpectPdRatio(ratio, pRate, dRate);
    } else if (input.type == DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO) {
        ret = ClusterExpectPdflexRatio(ratio, pRate, dRate);
    } else {
        LOG_E("[RoleManager] invalide ratio type");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (ret != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::ClusterExpectPdRatio(const DIGSGroupPDRatio &ratio, uint64_t &pRate,
                                                   uint64_t &dRate) const
{
    auto prefillAbility = ratio.pAbility;
    auto decodeAbility = ratio.dAbility;
    // min resource loss in each pd ratio
    double minResLoss = -1.0;
    size_t pInstance = 1;
    size_t dInstance = 1;
    for (size_t prefill = 1; prefill < MAX_INSTANCE_NUM; prefill++) {
        // min resource loss in each d num
        double resourceLoss = -1;
        size_t decodeTemp = 1;
        for (size_t decode = 1; decode <= MAX_INSTANCE_NUM - prefill; decode++) {
            // ignore transfer limit
            double subAcc =
                fabs(static_cast<double>(prefill) * prefillAbility - static_cast<double>(decode) * decodeAbility);
            if (resourceLoss < 0 || common::DoubleGreater(resourceLoss, subAcc)) {
                resourceLoss = subAcc;
                decodeTemp = decode;
            }
        }
        if (minResLoss < 0 || common::DoubleGreater(minResLoss, resourceLoss)) {
            minResLoss = resourceLoss;
            pInstance = prefill;
            dInstance = decodeTemp;
        }
    }
    pRate = pInstance;
    dRate = dInstance;
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::ClusterExpectPdflexRatio(DIGSGroupPDRatio &ratio, uint64_t &pRate, uint64_t &dRate) const
{
    // PDFlex场景，不考虑Flex实例，按照PD配比资源损耗计算期望配比。
    return ClusterExpectPdRatio(ratio, pRate, dRate);
}

int32_t ProportionCalculator::InitBestRatioWithExternInput(const ProportionInput &input, DIGSGroupPDRatio &ratio)
{
    int32_t ret;
    if (input.type == DIGSRatioType::DIGS_ROLE_PD_RATIO) {
        ret = InitPdBestRatioWithExternInput(input, ratio);
    } else if (input.type == DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO) {
        ret = InitPdflexBestRatioWithExternInput(input, ratio);
    } else {
        LOG_E("[RoleManager] invalide ratio type");
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }

    if (ret != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    if (SaveRatio(ratio, input.type) != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }

    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::InitPdBestRatioWithExternInput(const ProportionInput &input,
                                                             DIGSGroupPDRatio &ratio) const
{
    double pAbility = ratio.pAbility;
    double dAbility = ratio.dAbility;
    double pAbilityAll = 0.0;
    double dAbilityAll = 0.0;
    uint64_t alloc2DInstance = 0;
    uint64_t alloc2PInstance = 0;
    while (alloc2PInstance + alloc2DInstance < input.instanceNum) {
        if (pAbilityAll <= dAbilityAll) {
            alloc2PInstance++;
            pAbilityAll = pAbilityAll + pAbility * 1;
        } else {
            alloc2DInstance++;
            dAbilityAll = dAbilityAll + dAbility * 1;
        }
    }
    ratio.pNum = alloc2PInstance;
    ratio.dNum = alloc2DInstance;
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::InitPdflexBestRatioWithExternInput(const ProportionInput &input,
                                                                 DIGSGroupPDRatio &ratio) const
{
    int32_t ret = CalPdflexNum(input.instanceNum, input.flexInstNum, ratio);
    if (ret != static_cast<int32_t>(common::Status::OK)) {
        return ret;
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ProportionCalculator::SaveRatio(DIGSGroupPDRatio &ratio, DIGSRatioType type)
{
    int32_t ret = memcpy_s(&ratio_, sizeof(DIGSGroupPDRatio), &ratio, sizeof(DIGSGroupPDRatio));
    if (ret != EOK) {
        LOG_E("[RoleManager] ratio_ memcpy failed. errcode: " + ret);
        return static_cast<int32_t>(common::Status::STATISTICAL_ERROR);
    }
    ret = memcpy_s(&ratioPre_, sizeof(DIGSGroupPDRatio), &ratio, sizeof(DIGSGroupPDRatio));
    if (ret != EOK) {
        LOG_E("[RoleManager] ratioPre_ memcpy failed. errcode: " + ret);
        return static_cast<int32_t>(common::Status::STATISTICAL_ERROR);
    }
    if (type == DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO) {
        double tp;
        double td;
        CalPdflexThroughput(ratio, tp, td);
        double throughput = common::MinDouble(tp, td);
        throughput_ = throughput;
        throughputPrev_ = throughput;
    }
    return static_cast<int32_t>(common::Status::OK);
}

void ProportionCalculator::CalSingleInstanceAbility(MINDIE::MS::DIGSRequestSummary summary, double &pInstanceAbility,
                                                    double &dInstanceAbility) const
{
    DIGSGroupPDRatio digsGroupPdRatio;
    simulation::SimulationCalculator::GetInstance().CalAbility(summary, digsGroupPdRatio);
    pInstanceAbility = digsGroupPdRatio.pAbility;
    dInstanceAbility = digsGroupPdRatio.dAbility;
}

}