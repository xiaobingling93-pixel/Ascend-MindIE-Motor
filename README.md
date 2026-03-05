# MindIE Motor

## 🔥Latest News

[2025/12] MindIE Motor正式开源。

## 🚀简介

MindIE Motor是面向LLM PD分离推理的请求调度框架，通过开放、可扩展的推理服务化平台架构提供推理服务化能力，向下对接MindIE LLM，满足大语言模型高性能推理需求。MindIE Motor提供以下两个方面的能力：

-   PD分离的请求调度：主要将外部的客户请求分发到负载最低的Prefill/Decode实例上，起到负载均衡的作用；
-   RAS（Reliability、Availability和Serviceability）：增强PD分离服务可靠性、可用性和可服务性的能力。

MindIE Motor及其周边组件交互架构图如下所示。

**图 1** MindIE Motor架构图<a id="fig186975591945"></a>  
![](./docs/zh/figures/mindie_motor_architectural_diagram.png)

MindIE Motor提供PD分离推理服务化调度和RAS能力，关键组件和模块解释如下。
-  Coordinator：调度器。是用户推理请求的入口，接收高并发的推理请求，进行请求调度、请求管理、请求转发等，是整个集群的数据请求入口。
    -   Endpoint：对外RESTful接口，如OpenAI接口。
    -   Metrics：PD分离服务整体的Metrics统计指标，是整个服务的Prefill/Decode实例的统计指标汇总。
    -   Controller Monitor：接收Controller同步的实例状态信息，如健康状态、故障实例等信息。
    -   LoadBalancer：负载均衡调度。
    -   RequestMonitor：请求状态监控，如请求阶段、请求异常等。

-   Controller：控制器。完成集群内所有Prefill/Decode实例的业务状态管控、PD身份管理与决策、RAS能力等，是整个集群的状态管控器和决策大脑。
    -   FaultManager：故障管理模块，接收上报的故障，并处理故障，如隔离、重启、自愈恢复等。
     -   InsManager：实例管理器，负责PD实例身份的分配、调整等。
     -   CCAEReporter：运维管理信息上报，上报PD实例、Metrics等统计信息。
     -   InsMonitor：PD实例监控，包括心跳、负载等。

-   MindIE LLM：提供单个模型服务实例（Prefiller/Decoder）服务化推理能力，提供ContinuousBatching、PagedAttention、投机推理等LLM加速特性。
-   ClusterD：MindCluster高阶组件，负责故障诊断和全局RankTable表（整个PD分离服务所需的组网和Device信息）下发等功能。
-   CCAE：算存网一体化运维可视化平台。

**MindIE Motor是面向通用模型场景的推理服务化框架，通过开放、可扩展的推理服务化平台架构提供推理服务化能力，支持对接业界主流推理框架接口，满足大语言模型的高性能推理需求**。

以下是两个MindIE Motor代码仓库**智能体**，只需点击 "**Ask AI**" 徽章，即可进入其专属页面，有效缓解源码阅读的困难，开启智能代码学习与问答体验！它们将帮助您更深入地理解MindIE Motor的运行原理，并协助解决使用过程中遇到的问题与错误。

<div align="center">

[![Zread](https://img.shields.io/badge/Zread-Ask_AI-_.svg?style=flat&color=0052D9&labelColor=000000&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTQuOTYxNTYgMS42MDAxSDIuMjQxNTZDMS44ODgxIDEuNjAwMSAxLjYwMTU2IDEuODg2NjQgMS42MDE1NiAyLjI0MDFWNC45NjAxQzEuNjAxNTYgNS4zMTM1NiAxLjg4ODEgNS42MDAxIDIuMjQxNTYgNS42MDAxSDQuOTYxNTZDNS4zMTUwMiA1LjYwMDEgNS42MDE1NiA1LjMxMzU2IDUuNjAxNTYgNC45NjAxVjIuMjQwMUM1LjYwMTU2IDEuODg2NjQgNS4zMTUwMiAxLjYwMDEgNC45NjE1NiAxLjYwMDFaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00Ljk2MTU2IDEwLjM5OTlIMi4yNDE1NkMxLjg4ODEgMTAuMzk5OSAxLjYwMTU2IDEwLjY4NjQgMS42MDE1NiAxMS4wMzk5VjEzLjc1OTlDMS42MDE1NiAxNC4xMTM0IDEuODg4MSAxNC4zOTk5IDIuMjQxNTYgMTQuMzk5OUg0Ljk2MTU2QzUuMzE1MDIgMTQuMzk5OSA1LjYwMTU2IDE0LjExMzQgNS42MDE1NiAxMy43NTk5VjExLjAzOTlDNS42MDE1NiAxMC42ODY0IDUuMzE1MDIgMTAuMzk5OSA0Ljk2MTU2IDEwLjM5OTlaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik0xMy43NTg0IDEuNjAwMUgxMS4wMzg0QzEwLjY4NSAxLjYwMDEgMTAuMzk4NCAxLjg4NjY0IDEwLjM5ODQgMi4yNDAxVjQuOTYwMUMxMC4zOTg0IDUuMzEzNTYgMTAuNjg1IDUuNjAwMSAxMS4wMzg0IDUuNjAwMUgxMy43NTg0QzE0LjExMTkgNS42MDAxIDE0LjM5ODQgNS4zMTM1NiAxNC4zOTg0IDQuOTYwMVYyLjI0MDFDMTQuMzk4NCAxLjg4NjY0IDE0LjExMTkgMS42MDAxIDEzLjc1ODQgMS42MDAxWiIgZmlsbD0iI2ZmZiIvPgo8cGF0aCBkPSJNNCAxMkwxMiA0TDQgMTJaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00IDEyTDEyIDQiIHN0cm9rZT0iI2ZmZiIgc3Ryb2tlLXdpZHRoPSIxLjUiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPgo8L3N2Zz4K&logoColor=ffffff)](https://zread.ai/verylucky01/MindIE-Motor)&nbsp;&nbsp;&nbsp;&nbsp;
[![DeepWiki](https://img.shields.io/badge/DeepWiki-Ask_AI-_.svg?style=flat&color=0052D9&labelColor=000000&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACwAAAAyCAYAAAAnWDnqAAAAAXNSR0IArs4c6QAAA05JREFUaEPtmUtyEzEQhtWTQyQLHNak2AB7ZnyXZMEjXMGeK/AIi+QuHrMnbChYY7MIh8g01fJoopFb0uhhEqqcbWTp06/uv1saEDv4O3n3dV60RfP947Mm9/SQc0ICFQgzfc4CYZoTPAswgSJCCUJUnAAoRHOAUOcATwbmVLWdGoH//PB8mnKqScAhsD0kYP3j/Yt5LPQe2KvcXmGvRHcDnpxfL2zOYJ1mFwrryWTz0advv1Ut4CJgf5uhDuDj5eUcAUoahrdY/56ebRWeraTjMt/00Sh3UDtjgHtQNHwcRGOC98BJEAEymycmYcWwOprTgcB6VZ5JK5TAJ+fXGLBm3FDAmn6oPPjR4rKCAoJCal2eAiQp2x0vxTPB3ALO2CRkwmDy5WohzBDwSEFKRwPbknEggCPB/imwrycgxX2NzoMCHhPkDwqYMr9tRcP5qNrMZHkVnOjRMWwLCcr8ohBVb1OMjxLwGCvjTikrsBOiA6fNyCrm8V1rP93iVPpwaE+gO0SsWmPiXB+jikdf6SizrT5qKasx5j8ABbHpFTx+vFXp9EnYQmLx02h1QTTrl6eDqxLnGjporxl3NL3agEvXdT0WmEost648sQOYAeJS9Q7bfUVoMGnjo4AZdUMQku50McDcMWcBPvr0SzbTAFDfvJqwLzgxwATnCgnp4wDl6Aa+Ax283gghmj+vj7feE2KBBRMW3FzOpLOADl0Isb5587h/U4gGvkt5v60Z1VLG8BhYjbzRwyQZemwAd6cCR5/XFWLYZRIMpX39AR0tjaGGiGzLVyhse5C9RKC6ai42ppWPKiBagOvaYk8lO7DajerabOZP46Lby5wKjw1HCRx7p9sVMOWGzb/vA1hwiWc6jm3MvQDTogQkiqIhJV0nBQBTU+3okKCFDy9WwferkHjtxib7t3xIUQtHxnIwtx4mpg26/HfwVNVDb4oI9RHmx5WGelRVlrtiw43zboCLaxv46AZeB3IlTkwouebTr1y2NjSpHz68WNFjHvupy3q8TFn3Hos2IAk4Ju5dCo8B3wP7VPr/FGaKiG+T+v+TQqIrOqMTL1VdWV1DdmcbO8KXBz6esmYWYKPwDL5b5FA1a0hwapHiom0r/cKaoqr+27/XcrS5UwSMbQAAAABJRU5ErkJggg==)](https://deepwiki.com/verylucky01/MindIE-Motor)

</div>



## 🔍目录结构
```
├── docs                                     # 项目文档                         
├── mindie_motor                             # 服务框架总模块
│   ├── python                               # Python封装与脚本
│   ├── src                                  # 服务管理模块
│   │   ├── common                           # 公共代码
│   │   ├── config                           # 配置文件
│   │   ├── controller                       # 控制器
│   │   ├── coordinator                      # 调度器
│   │   ├── example                          # 部署与样例
│   │   ├── http_client_ctl                  # HTTP客户端与管控
│   │   ├── node_health_management           # 节点状态探针
│   │   ├── test                             # 单元测试
├── module                                   # MindIE-LLM推理引擎模块
├── third_party                              # 第三方依赖
├── README.md   
```

## ⚡️版本说明

|MindIE软件版本|CANN版本兼容性|
|:---|:---|
|3.0.0|9.0.0|

## ⚡️环境部署

MindIE Motor安装前的相关软硬件环境准备，以及安装步骤，请参见[安装指南](./docs/zh/user_guide/install/installation_description.md)。


## ⚡️快速入门

  快速体验启动服务、接口调用、精度&性能测试和停止服务全流程，请参见[快速入门](./docs/zh/User_Guide/quick_start.md)。

## 📝学习文档

- [集群服务部署](./docs/zh/user_guide/service_deployment/)：介绍MindIE Motor集群服务部署方式，包括单机（非分布式）服务部署和PD分离单、多机服务部署。
- [集群管理组件](./docs/zh/user_guide/cluster_management_component/)：介绍MindIE Motor集群管理组件，包括Controller和Coordinator。
- [服务化接口](./docs/zh/user_guide/service_oriented_interface/)：介绍MindIE Motor提供的用户侧接口和集群内通信接口。
- [配套工具](./docs/zh/user_guide/service_oriented_optimization_tool.md)：介绍MindIE Motor提供的配套工具，包括性能/精度测试工具、MindIE探针工具、服务化调优工具、CertTools、OM Adapter和Node Manager。

## 📝免责声明

版权所有© 2025 MindIE Project.

您对"本文档"的复制、使用、修改及分发受知识共享（Creative Commmons）署名——相同方式共享4.0国际公共许可协议（以下简称"CC BY-SA 4.0"）的约束。为了方便用户理解，您可以通过访问https://creativecommons.org/licenses/by-sa/4.0/了解CC BY-SA 4.0的概要（但不是替代）。CC BY-SA 4.0的完整协议内容您可以访问如下网址获取：https://creativecommons.org/licenses/by-sa/4.0/legalcode。

## 📝贡献指南

1. 提交错误报错：如果您在MindIE Motor中发现了一个不存在安全问题的漏洞，请在MindIE Motor仓库中的Issues中搜索，以防该漏洞被重复提交，如果找不到漏洞可以创建一个新的Issues。如果发现了一个安全问题请不要将其公开，请参阅安全问题处理方式。提交错误报告时应该包含完整信息。
2. 安全问题处理：本项目中对安全问题处理的形式，请通过邮箱通知项目核心人员确认编辑。
3. 解决现有问题：通过查看仓库的Issues列表可以发现需要处理的问题信息，可以尝试解决其中的某个问题。
4. 如何提供新功能：请使用Issues的Feature标签进行标记，我们会定期处理和确认开发。
5. 开始贡献：
   1. Fork本项目的仓库。
   2. Clone到本地。
   3. 创建开发分支。
   4. 本地测试：提交前请通过所有单元测试，包括新增的测试用例。
   5. 提交代码。
   6. 新建Pull Request。
   7. 代码检视：您需要根据评审意见修改代码，并重新提交更新。此流程可能涉及多轮迭代。
   8. 当您的PR获得足够数量的检视者批准后，Committer会进行最终审核。
   9. 审核和测试通过后，CI会将您的PR合并到项目的主干分支。
 
更多贡献相关文档请参见[贡献指南](./contributing.md)。

## 📝相关信息

- [安全声明](./security.md)
- [LICENSE](./LICENSE.md)
