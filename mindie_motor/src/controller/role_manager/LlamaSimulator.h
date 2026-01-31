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
#ifndef MINDIE_DIGS_LLAMASIMULATOR_H
#define MINDIE_DIGS_LLAMASIMULATOR_H

#include <climits>
#include "parse_json.h"
#include "common.h"
#include "Logger.h"


namespace MINDIE::MS::simulation {
using Config = std::map<std::string, std::string>;

class LlamaSimulator {
public:
    explicit LlamaSimulator(Config& config);

    static int32_t Create(Config& config, std::unique_ptr<LlamaSimulator> &modelSimulator);

    void CalAbility(MINDIE::MS::DIGSRequestSummary summary, DIGSGroupPDRatio &digsGroupPdRatio);

    ~LlamaSimulator() = default;
    double CalTransferTime(const size_t& tokenLength) const;
    double CalTransferAbility(const size_t& tokenLength) const;

private:
    void InitLlamaSimulator(Config& config);

    static void SetSimulator(std::unique_ptr<LlamaSimulator> &simulator,
                             const Json &modelConfig, const Json &hardwareConfig);
    
    double CalTime(size_t batchSize, bool isDecode) const;

    double CalFetchLatency(size_t batchSize, bool isDecode) const;

    double CalComputeLatency(size_t batchSize, bool isDecode) const;

    double CalCommunicateLatency(size_t batchSize, bool isDecode) const;

    double CalModelWeightMem() const;

    double CalPerTokenMem() const;

    double CalMemUsage(size_t batchSize, bool isDecode) const;

    size_t CalBatchSize(bool isDecode) const;

private:
    size_t seqInput_ = 0;
    size_t seqOutput_ = 0;
    double prefillSLOMS_ = 0.0;
    double decodeSLOMS_ = 0.0;
    size_t tensorParallel_ = 0;
    size_t pipeParallel_ = 0;
    size_t hardwareCardNums_ = 0;

    double numHiddenLayers_ = 0.0; // 隐藏层数
    double hiddenSize_ = 0.0; // 隐藏层维度
    double numAttentionHeads_ = 0.0; // 注意力头数
    double numKeyValueHeads_ = 0.0; // 键值头数
    double intermediateSize_ = 0.0; // 前馈网络隐藏层维度
    double byteSize_ = 0.0; // 字节大小

    double bandwidthGB_ = 0.0; // 通信带宽, 单位为GB
    double bandwidthEff_ = 0.0; // 通信效率
    double bandwidthRDMAGb_ = 0.0; // RDMA带宽, 单位为GB
    double tFlops_ = 0.0; // 计算能力, 单位为TFLOPS
    double tFlopsEff_ = 0.0; // 计算效率
    double mBandwidthTB_ = 0.0; // 访存带宽, 单位为TB
    double mBandwidthEff_ = 0.0; // 访存效率
    double memCapacity_ = 0.0; // 显存, 单位为GB
    double graphicsOutOfMemoryThreshold_ = 0.0; // 显存OOM水线
    double alpha_ = 0.0; // 启动时延, 单位为us
    double staticTransferDelay_ = 0.0; // 静态传输延迟
};

} // digs
#endif
