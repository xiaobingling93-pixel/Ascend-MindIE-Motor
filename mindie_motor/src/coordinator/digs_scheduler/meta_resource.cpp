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
#include <regex>
#include "meta_resource.h"

namespace MINDIE::MS {

std::vector<uint64_t> MetaResource::defaultAttr_ = {1, 0};

std::vector<std::string> MetaResource::attrName_ = {"slots", "blocks"};

// default weights: a, i, o, n, k, m, r, b, c; a * sum,
// input * i => input, input + o => inst seq, N = num(inst seq) + cur seq,
// C = N / n, K = N % n, sum = last(C) * n + last(N)/2 * K if K <= k else last(N) * K,
// m * max slots(max batch size), r * reported blocks, b * slots(batch size), c * blocks
std::vector<double> MetaResource::resWeight_ = {0, 0.22, 1024, 24, 6, 0, 1, 0, 1};

const size_t ATTR_SLOTS = 0;
const size_t ATTR_BLOCKS = 1;
const size_t SPEC_WEIGHTS = 7;

const size_t ATTR_A = 0;
const size_t ATTR_O = 2;
const size_t ATTR_N = 3;
const size_t ATTR_K = 4;
const size_t ATTR_M = 5;
const size_t ATTR_R = 6;

MetaResource::MetaResource()
{
    attributes_ = defaultAttr_;
}

MetaResource::MetaResource(uint64_t defaultValue)
{
    attributes_.reserve(defaultAttr_.size());
    for (size_t i = 0; i < defaultAttr_.size(); i++) {
        attributes_.push_back(defaultValue);
    }
}

bool MetaResource::CompareTo(const MetaResource& other)
{
    if (attributes_.size() != other.Size()) {
        return false;
    }

    for (size_t i = 0; i < attributes_.size(); ++i) {
        // 资源不满足要求
        if (attributes_[i] < other.attributes_[i]) {
            return false;
        }
    }
    // 满足要求
    return true;
}

bool MetaResource::IncResource(const MetaResource& other)
{
    if (attributes_.size() != other.Size()) {
        return false;
    }

    for (size_t i = 0; i < attributes_.size(); ++i) {
        attributes_[i] = attributes_[i] + other.attributes_[i];
    }

    computeAttributes_.insert(other.computeAttributes_.begin(),
                              other.computeAttributes_.end());
    return true;
}

bool MetaResource::DecResource(MetaResource& other)
{
    if (this->CompareTo(other)) {
        for (size_t i = 0; i < attributes_.size(); ++i) {
            attributes_[i] = attributes_[i] - other.attributes_[i];
        }

        for (auto& seqInput : other.computeAttributes_) {
            auto it = std::find(computeAttributes_.begin(), computeAttributes_.end(), seqInput);
            if (it != computeAttributes_.end()) {
                computeAttributes_.erase(it);
            }
        }
        return true;
    }
    return false;
}

uint64_t MetaResource::Slots()
{
    return attributes_[ATTR_SLOTS];
}

uint64_t MetaResource::Blocks()
{
    return attributes_[ATTR_BLOCKS];
}

uint64_t MetaResource::UpdateBlocks(uint64_t updateValue)
{
    auto res = attributes_[ATTR_BLOCKS];
    attributes_[ATTR_BLOCKS] = updateValue;
    return res;
}

void MetaResource::UpdateTokens(uint64_t updateValue)
{
    computeAttributes_.insert(updateValue);
}

void MetaResource::InitAttrs(std::string& names, std::string& attrs, std::string& weights)
{
    const char* split = ",";
    std::istringstream namesStream(names);
    std::istringstream attrsStream(attrs);
    std::string name;
    std::string attr;

    while (std::getline(namesStream, name, *split)) {
        if (!name.empty()) {
            attrName_.push_back(name);
        }
    }

    std::vector<uint64_t> tmpAttrs;
    while (std::getline(attrsStream, attr, *split)) {
        if (!attr.empty()) {
            try {
                tmpAttrs.push_back(std::stoul(attr));
            } catch (const std::exception &e) {
                LOG_E("[%s] [DIGS] Invalid input attribute. Please input from the beginning.",
                      GetErrorCode(ErrorType::INVALID_INPUT, CommonFeature::DIGS).c_str());
                continue;
            } catch (...) {
                LOG_E("[%s] [DIGS] Invalid input attribute. Please input from the beginning.",
                      GetErrorCode(ErrorType::INVALID_INPUT, CommonFeature::DIGS).c_str());
                continue;
            }
        }
    }

    if (!tmpAttrs.empty() && tmpAttrs.size() == attrName_.size()) {
        LOG_I("DIGS: meta resource init attribute %s %s",
              common::Dims2Str(&attrName_[0], attrName_.size()).c_str(),
              common::Dims2Str(&tmpAttrs[0], tmpAttrs.size()).c_str());
        defaultAttr_ = std::move(tmpAttrs);
    } else {
        LOG_W("[%s] [DIGS] AttributeS initialze error, use default",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
    }
    MetaResource::InitWeights(weights);
}

void MetaResource::InitWeights(std::string& weights)
{
    const char* split = ",";
    std::istringstream weightsStream(weights);
    std::string weight;
    std::vector<double> tmpWeights;
    while (std::getline(weightsStream, weight, *split)) {
        double res = 0;
        if (common::StrToDouble(weight, "meta resource weight", res) != static_cast<int32_t>(common::Status::OK)) {
            continue;
        }
        tmpWeights.push_back(res);
    }
    LOG_I("DIGS: meta resource init weights %s %s",
          common::Dims2Str(&attrName_[0], attrName_.size()).c_str(),
          common::Dims2Str(&tmpWeights[0], tmpWeights.size()).c_str());
    auto weightsSize = resWeight_.size() - defaultAttr_.size() + attrName_.size();
    if (tmpWeights.size() < weightsSize) {
        LOG_W("[%s] [DIGS] Weights init error, use default",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        if (weightsSize != resWeight_.size()) {
            resWeight_.resize(weightsSize, 1);
        }
        return;
    }
    resWeight_ = std::move(tmpWeights);
}

int32_t MetaResource::ResMul(std::unique_ptr<MetaResource> &result, const std::unique_ptr<MetaResource>& src,
                             double_t mul)
{
    auto copy = std::make_unique<MetaResource>(src);
    if (std::fabs(mul - 1) > common::DOUBLE_EPS) {
        for (size_t i = 0; i < copy->Size(); i++) {
            double_t product = static_cast<double_t>(copy->attributes_[i]) * mul;
            if (std::isfinite(product) && product >= 0.0 &&
                product < static_cast<double_t>(std::numeric_limits<uint64_t>::max())) {
                copy->attributes_[i] = static_cast<uint64_t>(product);
            } else {
                copy->attributes_[i] = 0;
                LOG_E("[%s] [DIGS] Invalid multiplication result for resource attribute: %s",
                      GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str(),
                      std::to_string(product).c_str());
                return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
            }
        }
    }
    result = std::move(copy);
    return static_cast<int32_t>(common::Status::OK);
}

uint64_t MetaResource::TotalLoad(const std::unique_ptr<MetaResource>& res)
{
    uint64_t sum = 0;
    for (auto attr : res->attributes_) {
        sum += attr;
    }
    return sum;
}

double MetaResource::ComputeAwareLoad(const std::unique_ptr<MetaResource>& res, size_t maxSlots, size_t reportedBlocks,
                                      const std::unique_ptr<MetaResource>& demand)
{
    size_t tokenSum = MetaResource::GetTokenSum(res, maxSlots, demand);
    double tokensScore = static_cast<double>(tokenSum) * MetaResource::ResWeight()[ATTR_A];

    // 计算max(Batch size) * b
    if (res->Slots() == maxSlots) {
        maxSlots += 1;
    }
    double maxSlotsScore = static_cast<double>(maxSlots) * resWeight_[ATTR_M];

    // 计算实例实际上报的blocks * r
    double blocksScore = static_cast<double>(reportedBlocks) * resWeight_[ATTR_R];

    // 计算c * (Batch Size)i和d * Σ(Block Num)ij
    double load = 0;
    for (size_t i = 0; i < MetaResource::AttrName().size(); i++) {
        load += static_cast<double>(res->At(i)) * MetaResource::ResWeight()[i + SPEC_WEIGHTS];
        load += static_cast<double>(demand->At(i)) * MetaResource::ResWeight()[i + SPEC_WEIGHTS];
    }

    return tokensScore + maxSlotsScore + blocksScore + load;
}

size_t MetaResource::GetTokenSum(const std::unique_ptr<MetaResource>& res, size_t maxSlots,
                                 const std::unique_ptr<MetaResource>& demand)
{
    // 获取并处理参数
    auto o = static_cast<int64_t>(MetaResource::ResWeight()[ATTR_O]);
    o = o < 0 ? 0 : o;
    auto n = static_cast<int64_t>(MetaResource::ResWeight()[ATTR_N]);
    n = n <= 0 ? 1 : n;
    auto k = static_cast<size_t>(MetaResource::ResWeight()[ATTR_K]);

    size_t tokenSum = 0;
    const size_t slotsCoefficient = 2; // 用于max slots和n比较时乘以的系数
    const size_t tailCoefficient = 2; // 尾块处理时乘以的系数

    size_t demandSeqInput = 0;
    if (demand->computeAttributes_.size() == 1) {
        demandSeqInput = *demand->computeAttributes_.begin();
    }

    if (common::DoubleIsZero(MetaResource::ResWeight()[ATTR_A])) {
        tokenSum = 1;
    } else if (maxSlots < n / slotsCoefficient) {
        for (auto& seqInputTokens : res->computeAttributes_) {
            tokenSum += seqInputTokens + o;
        }
        tokenSum += demandSeqInput;
    } else {
        // 更新实例中seq len，每一项加上偏移o
        std::multiset<size_t> tmpComAttr;
        std::transform(
            res->computeAttributes_.begin(), res->computeAttributes_.end(),
            std::inserter(tmpComAttr, tmpComAttr.begin()),
            [o](uint64_t val) { return val + o; }
        );
        tmpComAttr.insert(demandSeqInput);
        // 计算tokenScore
        size_t seqNum = tmpComAttr.size();
        size_t groupNum  = seqNum / n;
        size_t tailSeqNum = seqNum % n;

        size_t groupIdx = 0;
        size_t innerCount = 0;
        for (auto& seqTokens : tmpComAttr) {
            if (groupNum  > 0 && groupIdx < groupNum) {
                // 处理前面组，只取每组最后一个
                innerCount++;
                if (innerCount % n == 0) {
                    tokenSum += seqTokens * n;
                    groupIdx++;
                }
            } else {
                // 处理尾块
                innerCount++;
                if (innerCount == tailSeqNum) {
                    tokenSum += tailSeqNum <= k ? (seqTokens / tailCoefficient * tailSeqNum) :
                                                    (seqTokens * tailSeqNum);
                }
            }
        }
    }
    return tokenSum;
}

}
