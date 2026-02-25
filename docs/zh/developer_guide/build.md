# MindIE-Motor 编译、安装与运行指南

本文档介绍如何从源码编译 MindIE-Motor，生成 `.whl` 包，安装与运行。

---

## 1. 环境准备

### 1.1 安装 Python 工具

MindIE-Motor 支持 **Python == 3.11**。

安装 Python 编译所需工具：

```bash
pip install --upgrade pip
pip install wheel setuptools
```

### 1.2 克隆源码仓库

```bash
git clone https://gitcode.com/Ascend/MindIE-Motor.git
cd MindIE-Motor
```

---

## 2. 编译 MindIE-Motor

### 2.1 编译第三方依赖

如果没有提前准备第三方依赖，需要先执行：

```bash
bash build/build.sh -d 3rd -b 3rd
```


### 2.2 设置环境变量

获取 Python site-packages 路径（建议不要硬编码 torch 路径），并配置动态库搜索路径：

```bash
TORCH_PATH=$(python3 -c "import torch, os; print(os.path.dirname(torch.__file__))")
export LD_LIBRARY_PATH=${TORCH_PATH}/lib:${TORCH_PATH}/../torch.libs:$LD_LIBRARY_PATH
```

可选：指定生成 `.whl` 包的版本号：

```bash
export MINDIE_MOTOR_VERSION_OVERRIDE=3.0.0
```

### 2.3 编译生成 `.whl` 包

在setup.py文件目录执行：

```bash
cd mindie_motor/python
pip wheel . --no-build-isolation -v
```

* 编译完成后，会在当前目录生成 `mindie_motor-<version>-*.whl` 文件。
* 编译时，`setup.py` 会自动调用 `build.sh` 编译C++代码，并拷贝第三方依赖到包内。
* 编译后，生成临时目录 `build`、存放二进制的目录 `output` 和debug符号表`motor_debug_symbols` 目录。

---

## 3. 安装 MindIE-Motor

### 3.1 安装 `.whl` 包

```bash
pip install mindie_motor*.whl
```

### 3.2 权限配置

由于whl包安装后是按照系统设定的权限，如下权限可能需要适配（可根据实际权限报错处理），在boot.sh中添加命令：

```bash
chmod 500 /usr/local/lib/python3.11/site-packages/mindie_motor/scripts/http_client_ctl/*;
chmod 550 /usr/local/lib/python3.11/site-packages/mindie_motor/examples/kubernetes_deploy_scripts/boot_helper/*; 
chmod 640 /usr/local/lib/python3.11/site-packages/mindie_motor/examples/kubernetes_deploy_scripts/boot_helper/boot.sh; 
chmod 500 /usr/local/lib/python3.11/site-packages/mindie_motor/scripts/http_client_ctl/*; 
chmod 700 /root; 
chmod 640 /usr/local/lib/python3.11/site-packages/mindie_motor/conf/model_config/*.json;
chmod 640 /usr/local/lib/python3.11/site-packages/mindie_motor/conf/machine_config/*.json;
```

注：3.1,3.2章节命令可以统一添加到boot.sh内，样例参考如下：

![image.png](https://raw.gitcode.com/user-images/assets/8787468/64b35e06-00ab-45f3-9cb1-1f56068b2ac0/image.png 'image.png')
---

## 4. 运行 MindIE Motor

1. 查询安装路径：

```bash
pip show mindie_motor | grep Location
```

2. 找到配置文件路径：

将查询到的路径替换 `<site-packages>`，

```
<site-packages>/mindie_motor/conf/ms_controllerg.json
<site-packages>/mindie_motor/conf/ms_coordinator.json
<site-packages>/mindie_motor/examples/kubernetes_deploy_scripts/conf/node_manager.json
```

3. 修改配置文件：

* 如果之前没有 `umask`，请确保文件权限至少为 `640`，修改方法 `chmod 640 <site-packages>mindie_motor/(根据实际需求替换为相应路径)/xxx.json`。
* 根据实际需求修改 `xxx.json` 的各项参数。

4. 启动服务:

```bash
   cd <site-packages>/mindie_motor/examples/kubernetes_deploy_scripts
   export MINDIE_LOG_TO_STDOUT=1   # 可选：日志输出到屏幕
   python3 deploy_ac_job.py --user_config_path ./user_config_base_A3.json
```

5. 验证运行状态

```bash
   kubectl get pods -A -owide
```
执行命令后当屏幕显示mindie-controller、mindie-cooridnator和mindie-server的READY状态为"1/1"时，则说明服务启动成功。

## 5. 升级

安装新的 Python wheel 包，即可升级。

```
pip install mindie_motor-{version}-{python tag}-{abi tag}-{platform tag}.whl
```

如果是同版本，可以加上 `--force-reinstall` 强制重新安装。

> 注意：
> 重装mindie_motor过程中，安装目录（<site-packages>/mindie_motor）内容会被全部删除再重装新版本，如果配置文件、证书文件等需要保留，请先备份。

