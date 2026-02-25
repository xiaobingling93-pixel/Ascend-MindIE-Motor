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
#include "LlamaSimulator.h"

namespace MINDIE::MS::simulation {

constexpr double TIME_CHANGE = 1000;
constexpr double MEM_CHANGE = 1024;
constexpr double TFLOPS2FLOPS = 1e12;
constexpr double GB2GBIT = 8.0;
constexpr size_t MAX_TOKEN_NUM = 128;

constexpr double DEFAULT_PREFILL_SLOMS = 1000;
constexpr double DEFAULT_DECODE_SLOMS = 50;
constexpr size_t DEFAULT_TENSOR_PARALLEL = 8;
constexpr size_t DEFAULT_PIPE_PARALLEL = 1;
constexpr size_t DEFAULT_HARDWARE_CARDS = 8;
constexpr size_t MAX_BATCH_SIZE = 100000;
// 权重矩阵数量
constexpr double TWO_WEIGHT_MATRIX = 2.0;
constexpr double THREE_WEIGHT_MATRIX = 3.0;
// 多项式系数
constexpr double COEFFICIENT = 2.0;

static std::unordered_map<std::string, double> g_byteSizes {{"float16", 2}, {"bfloat16", 2}};

static std::set<std::string> requiredHardwareConfigKeys = {
    "BW_GB", "BWeff", "BW_RDMA_Gb", "TFLOPS",   "TFLOPSeff", "MBW_TB",
    "MBW_TBeff",     "MEMCapacity", "eta_OOM",    "alpha",        "staticTransferDelay"};

static std::set<std::string> requiredModelConfigKeys = {
    "hidden_size", "intermediate_size", "num_attention_heads", "num_hidden_layers",   "num_key_value_heads"};

LlamaSimulator::LlamaSimulator(Config& config)
    : prefillSLOMS_(DEFAULT_PREFILL_SLOMS),
      decodeSLOMS_(DEFAULT_DECODE_SLOMS),
      tensorParallel_(DEFAULT_TENSOR_PARALLEL),
      pipeParallel_(DEFAULT_PIPE_PARALLEL),
      hardwareCardNums_(DEFAULT_HARDWARE_CARDS)
{
    InitLlamaSimulator(config);
}

int32_t LlamaSimulator::Create(Config& config, std::unique_ptr<LlamaSimulator> &modelSimulator)
{
    std::string modelConfigString;
    std::string hardwareConfigString;
    if (!MINDIE::MS::common::GetConfig("model_params", modelConfigString, config) ||
        !MINDIE::MS::common::GetConfig("machine_params", hardwareConfigString, config)) {
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }

    Json modelConfig;
    Json hardwareConfig;
    try {
        modelConfig = Json::parse(modelConfigString);
        hardwareConfig = Json::parse(hardwareConfigString);
    } catch (const nlohmann::json::parse_error &e) {
        std::string str(e.what(), sizeof(e.what()) - 1);
        LOG_E("[%s] [DIGS] JSON parse configuration error, %s",
              GetErrorCode(ErrorType::EXCEPTION, CommonFeature::DIGS).c_str(),
              str.c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    if (modelConfig == nullptr || hardwareConfig == nullptr) {
        LOG_E("[%s] [DIGS] Config JSON is nullptr. Config string can not convert to JSON.",
              GetErrorCode(ErrorType::INVALID_INPUT, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }

    for (auto &key : requiredModelConfigKeys) {
        if (MINDIE::MS::common::Object2Double(modelConfig, key) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
    }
    if (modelConfig["torch_dtype"] == nullptr) {
        LOG_W("[%s] [DIGS] Model config 'torch_dtype' is not set.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        modelConfig["torch_dtype"] = "float16";
    }

    for (auto &key : requiredHardwareConfigKeys) {
        if (MINDIE::MS::common::Object2Double(hardwareConfig, key) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
    }
    try {
        modelSimulator = std::make_unique<LlamaSimulator>(config);
    } catch (const std::exception& e) {
        std::string str(e.what(), sizeof(e.what()) - 1);
        LOG_E("[%s] [DIGS] make unique llama simulator fail, %s",
              GetErrorCode(ErrorType::EXCEPTION, CommonFeature::DIGS).c_str(), str.c_str());
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }

    SetSimulator(modelSimulator, modelConfig, hardwareConfig);
    return static_cast<int32_t>(common::Status::OK);
}

void LlamaSimulator::InitLlamaSimulator(Config& config)
{
    double dItem = 0;
    if (MINDIE::MS::common::GetConfig("prefill_slo", dItem, config)) {
        prefillSLOMS_ = dItem;
    }
    if (MINDIE::MS::common::GetConfig("decode_slo", dItem, config)) {
        decodeSLOMS_ = dItem;
    }

    size_t ulItem = 0;
    if (MINDIE::MS::common::GetConfig("tp", ulItem, config)) {
        tensorParallel_ = ulItem;
    }
    if (MINDIE::MS::common::GetConfig("pp", ulItem, config)) {
        pipeParallel_ = ulItem;
    }
    if (MINDIE::MS::common::GetConfig("hardware_card_nums", ulItem, config)) {
        hardwareCardNums_ = ulItem;
    }
}

void LlamaSimulator::SetSimulator(std::unique_ptr<LlamaSimulator> &simulator,
                                  const Json &modelConfig, const Json &hardwareConfig)
{
    if (simulator == nullptr) {
        LOG_E("[%s] [DIGS] Llama simulator invaild param  simulator is nullptr!.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return;
    }
    simulator->numHiddenLayers_ = modelConfig["num_hidden_layers"];
    simulator->hiddenSize_ = modelConfig["hidden_size"];
    simulator->numAttentionHeads_ = modelConfig["num_attention_heads"];
    simulator->numKeyValueHeads_ = modelConfig["num_key_value_heads"];
    simulator->intermediateSize_ = modelConfig["intermediate_size"];
    try {
        simulator->byteSize_ = g_byteSizes.at(modelConfig["torch_dtype"]);
    } catch (const std::out_of_range& e) {
        LOG_W("[%s] [DIGS] Model parameter 'torch_dtype' is not set or set wrong type.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        simulator->byteSize_ = g_byteSizes["float16"];
    }

    simulator->bandwidthGB_ = hardwareConfig["BW_GB"];
    simulator->bandwidthEff_ = hardwareConfig["BWeff"];
    simulator->bandwidthRDMAGb_ = hardwareConfig["BW_RDMA_Gb"];
    simulator->tFlops_ = hardwareConfig["TFLOPS"];
    simulator->tFlopsEff_ = hardwareConfig["TFLOPSeff"];
    simulator->mBandwidthTB_ = hardwareConfig["MBW_TB"];
    simulator->mBandwidthEff_ = hardwareConfig["MBW_TBeff"];
    simulator->memCapacity_ = hardwareConfig["MEMCapacity"];
    simulator->graphicsOutOfMemoryThreshold_ = hardwareConfig["eta_OOM"];
    simulator->alpha_ = hardwareConfig["alpha"];
    simulator->staticTransferDelay_ = hardwareConfig["staticTransferDelay"];
}

void LlamaSimulator::CalAbility(MINDIE::MS::DIGSRequestSummary summary, DIGSGroupPDRatio &digsGroupPdRatio)
{
    seqInput_ = summary.inputLength;
    seqOutput_ = summary.outputLength;
    size_t prefillMaxBatchSize = CalBatchSize(false);
    size_t decodeMaxBatchSize = CalBatchSize(true);
    double prefillTime = CalTime(prefillMaxBatchSize, false);
    double decodeTime = CalTime(decodeMaxBatchSize, true);
    if (MINDIE::MS::common::DoubleIsZero(prefillTime) ||
        MINDIE::MS::common::DoubleIsZero(decodeTime) ||
        seqOutput_ == 0) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator ability calculation: division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        digsGroupPdRatio.pAbility = 0;
        digsGroupPdRatio.dAbility = 0;
        return;
    }

    digsGroupPdRatio.pAbility = prefillMaxBatchSize / prefillTime * TIME_CHANGE;
    digsGroupPdRatio.dAbility = decodeMaxBatchSize / decodeTime / seqOutput_ * TIME_CHANGE;
}

double LlamaSimulator::CalComputeLatency(size_t batchSize, bool isDecode) const
{
    if (MINDIE::MS::common::DoubleIsZero(numAttentionHeads_)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator compute latency calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }
    size_t size = seqInput_;
    double allTokensNum = static_cast<double>(seqInput_);
    if (isDecode) {
        size = batchSize;
        allTokensNum = static_cast<double>(seqInput_) + static_cast<double>(seqOutput_);
    }
    double attention = TWO_WEIGHT_MATRIX * hiddenSize_ * hiddenSize_ * numHiddenLayers_ * size +
                       TWO_WEIGHT_MATRIX * hiddenSize_ * hiddenSize_ * numHiddenLayers_ * numKeyValueHeads_ /
                       numAttentionHeads_ * size + COEFFICIENT * hiddenSize_ * numHiddenLayers_ * size * allTokensNum;
    double ffn = TWO_WEIGHT_MATRIX * hiddenSize_ * intermediateSize_ * numHiddenLayers_ * size;
    double calPower = tFlops_ * tFlopsEff_;
    if (tensorParallel_ == 0 || pipeParallel_ == 0 || MINDIE::MS::common::DoubleIsZero(calPower)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator compute latency calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }

    return COEFFICIENT * (attention + ffn) / tensorParallel_ / pipeParallel_ /
           (calPower * TFLOPS2FLOPS) * TIME_CHANGE;
}

double LlamaSimulator::CalFetchLatency(size_t batchSize, bool isDecode) const
{
    if (MINDIE::MS::common::DoubleIsZero(numAttentionHeads_)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator fetch latency calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }

    double attention = TWO_WEIGHT_MATRIX * hiddenSize_ * hiddenSize_ * numHiddenLayers_ +
                       TWO_WEIGHT_MATRIX * hiddenSize_ * hiddenSize_ * numHiddenLayers_ *
                       numKeyValueHeads_ / numAttentionHeads_;
    double ffn = TWO_WEIGHT_MATRIX * hiddenSize_ * intermediateSize_ * numHiddenLayers_;
    double fetchBandwidth = mBandwidthTB_ * mBandwidthEff_;
    if (tensorParallel_ == 0 || pipeParallel_ == 0 || MINDIE::MS::common::DoubleIsZero(fetchBandwidth)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator fetch latency calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }
    double kvCache = 0;
    if (isDecode) {
        double allTokenLength = static_cast<double>(seqInput_) * batchSize +
                                static_cast<double>(seqOutput_) * batchSize;
        kvCache = TWO_WEIGHT_MATRIX * allTokenLength * hiddenSize_ * numHiddenLayers_ *
                  numKeyValueHeads_ / numAttentionHeads_;
    }
    return byteSize_ * (attention + ffn + kvCache) / tensorParallel_ / pipeParallel_ /
           (fetchBandwidth * MEM_CHANGE * MEM_CHANGE * MEM_CHANGE * MEM_CHANGE) * TIME_CHANGE;
}

double LlamaSimulator::CalCommunicateLatency(size_t batchSize, bool isDecode) const
{
    double commBandwidth = bandwidthEff_ * bandwidthGB_;
    double commRDMABandwidth = bandwidthEff_ * bandwidthRDMAGb_;
    if (hardwareCardNums_ == 0 || MINDIE::MS::common::DoubleIsZero(commRDMABandwidth) ||
        MINDIE::MS::common::DoubleIsZero(commBandwidth)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator communicate latency calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }

    double tpInter = 0;
    double tpInterStatic = 0;
    auto hostNum = static_cast<size_t>(std::ceil(tensorParallel_ / hardwareCardNums_));
    if (hostNum > 0) {
        tpInter = COEFFICIENT * (hostNum - 1) * TWO_WEIGHT_MATRIX * byteSize_ * hiddenSize_
                * numHiddenLayers_ / hostNum / (commRDMABandwidth / GB2GBIT * MEM_CHANGE * MEM_CHANGE * MEM_CHANGE);
        tpInterStatic = TWO_WEIGHT_MATRIX * byteSize_ * (hostNum - 1) * alpha_ * numHiddenLayers_ / TIME_CHANGE;
    }

    size_t size = seqInput_;
    if (isDecode) {
        size = 1;
    }
    double tpIntra = TWO_WEIGHT_MATRIX * byteSize_ * size * hiddenSize_ * numHiddenLayers_ /
                     (commBandwidth * MEM_CHANGE * MEM_CHANGE * MEM_CHANGE) * TIME_CHANGE;
    double tpIntraStatic = TWO_WEIGHT_MATRIX * TWO_WEIGHT_MATRIX * alpha_ * numHiddenLayers_ / TIME_CHANGE;

    double ppIntra = 0;
    double ppStatic = 0;
    if (pipeParallel_ > 0) {
        ppIntra = (pipeParallel_ - 1) * size * hiddenSize_ * byteSize_ /
                  (commRDMABandwidth / GB2GBIT * MEM_CHANGE * MEM_CHANGE * MEM_CHANGE) * TIME_CHANGE;
        ppStatic = (pipeParallel_ - 1) * alpha_ / TIME_CHANGE;
    }

    double comm = tpInter + tpIntra + ppIntra;
    double commStatic = tpInterStatic + tpIntraStatic + ppStatic;

    return batchSize * comm + commStatic;
}

double LlamaSimulator::CalTime(size_t batchSize, bool isDecode) const
{
    double batchComputeMs = isDecode ?
        CalComputeLatency(batchSize, true) :
        batchSize * CalComputeLatency(batchSize, false);
    
    double batchCommMs = CalCommunicateLatency(batchSize, isDecode);
    double batchFetchMs = CalFetchLatency(batchSize, isDecode);
    
    return batchComputeMs + batchFetchMs + batchCommMs;
}

size_t LlamaSimulator::CalBatchSize(bool isDecode) const
{
    size_t batchSize = 1;
    double sloMs = isDecode ? decodeSLOMS_ : prefillSLOMS_;
    
    // 根据SLO计算最大batch size
    while (batchSize < MAX_BATCH_SIZE && sloMs >= CalTime(batchSize + 1, isDecode)) {
        batchSize++;
    }
    double limitForOOM = memCapacity_ * graphicsOutOfMemoryThreshold_;
    while (limitForOOM <= CalMemUsage(batchSize, isDecode)) {
        if (batchSize == 1) {
            break;
        }
        batchSize--;
    }

    return batchSize;
}

double LlamaSimulator::CalModelWeightMem() const
{
    if (tensorParallel_ == 0 || pipeParallel_ == 0 || MINDIE::MS::common::DoubleIsZero(numAttentionHeads_)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator model weight memory calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }

    double attention = byteSize_ * (TWO_WEIGHT_MATRIX * numHiddenLayers_ * hiddenSize_ * hiddenSize_ +
            TWO_WEIGHT_MATRIX * numHiddenLayers_ * hiddenSize_ * hiddenSize_ * numKeyValueHeads_ / numAttentionHeads_);
    double ffn = byteSize_ * THREE_WEIGHT_MATRIX * hiddenSize_ * intermediateSize_ * numHiddenLayers_;

    return (attention + ffn) / tensorParallel_ / pipeParallel_ / MEM_CHANGE / MEM_CHANGE / MEM_CHANGE;
}

double LlamaSimulator::CalPerTokenMem() const
{
    if (tensorParallel_ == 0 || pipeParallel_ == 0 || MINDIE::MS::common::DoubleIsZero(numAttentionHeads_)) {
        LOG_E("[%s] [DIGS] Invalid parameters for Llama simulator model weight memory calculation, "
              "division by zero encountered.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }

    return COEFFICIENT * byteSize_ * hiddenSize_ * numHiddenLayers_ * numKeyValueHeads_ / numAttentionHeads_
           / tensorParallel_ / pipeParallel_ / MEM_CHANGE / MEM_CHANGE;
}

double LlamaSimulator::CalMemUsage(size_t batchSize, bool isDecode) const
{
    double modelMem = CalModelWeightMem();
    double perTokenMem = CalPerTokenMem() / MEM_CHANGE;
    double tokenLength = static_cast<double>(batchSize) * seqInput_;
    if (isDecode) {
        tokenLength += static_cast<double>(batchSize) * seqOutput_;
    }

    return modelMem + perTokenMem * tokenLength;
}

double LlamaSimulator::CalTransferTime(const size_t &tokenLength) const
{
    double hiddenDimension = hiddenSize_;
    double layers = numHiddenLayers_;
    double kvHead = numKeyValueHeads_;
    double qHead = numAttentionHeads_;
    double byteSize = byteSize_;
    double memRDMAGB = bandwidthRDMAGb_ * bandwidthEff_ / GB2GBIT;
    double staticTransferDelay = staticTransferDelay_ * TIME_CHANGE;
    if (tensorParallel_ == 0 || pipeParallel_ == 0 || MINDIE::MS::common::DoubleIsZero(qHead) ||
        MINDIE::MS::common::DoubleIsZero(memRDMAGB)) {
        LOG_E("[" + GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS) + "] " +
                       "[RoleManager] Coefficient is zero, try to divide zero!");
        return 0;
    }
    double transferTime = COEFFICIENT * layers * tokenLength * byteSize * hiddenDimension * kvHead / qHead / memRDMAGB /
                          tensorParallel_ / pipeParallel_ * TIME_CHANGE / MEM_CHANGE / MEM_CHANGE / MEM_CHANGE;
    double staticDelay = COEFFICIENT * staticTransferDelay * layers * tokenLength / MAX_TOKEN_NUM;
    return transferTime + staticDelay;
}

double LlamaSimulator::CalTransferAbility(const size_t &tokenLength) const
{
    double transTime = CalTransferTime(tokenLength);
    if (MINDIE::MS::common::DoubleIsZero(transTime)) {
        return 0.0;
    }
    transTime /= TIME_CHANGE;
    return 1.0 / transTime;
}

} // digs
