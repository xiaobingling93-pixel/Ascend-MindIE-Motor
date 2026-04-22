#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#
# 优先检测 whl 包（mindie_motor_* 命令是否存在），再检测 run 包（ms_* 二进制）。
# 如果两者都没有就报错。
# 导出 MINDIE_IS_WHL_PACKAGE=1（whl）或 =0（run），供 node_manager / llm_daemon_starter 与 mindie_run_* 使用。

source "$MINDIE_LLM_HOME_PATH/set_env.sh"

_boot_dir=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)
detect_mies_install_path() {
    local cur="$1"
    local i
    for i in $(seq 1 16); do
        if [ -d "$cur/bin" ] && [ -d "$cur/conf" ]; then
            if [ -x "$cur/bin/ms_controller" ] || [ -x "$cur/bin/ms_coordinator" ]; then
                echo "$cur"
                return 0
            fi
        fi
        if [ -d "$cur/install/bin" ] && [ -d "$cur/install/conf" ]; then
            if [ -x "$cur/install/bin/ms_controller" ] || [ -x "$cur/install/bin/ms_coordinator" ]; then
                echo "$cur/install"
                return 0
            fi
        fi
        [ "$cur" = "/" ] && break
        cur=$(dirname "$cur")
    done
    return 1
}

if [ -z "${MIES_INSTALL_PATH:-}" ]; then
    if ! MIES_INSTALL_PATH=$(detect_mies_install_path "$_boot_dir"); then
        echo "Error: cannot detect MIES_INSTALL_PATH. Set it explicitly, e.g."
        echo "  export MIES_INSTALL_PATH=/usr/local/Ascend/mindie-service/<version>"
        exit 1
    fi
    export MIES_INSTALL_PATH
fi

if [ ! -d "$MIES_INSTALL_PATH/conf" ]; then
    echo "Error: MIES_INSTALL_PATH=$MIES_INSTALL_PATH has no conf/."
    exit 1
fi

# === 优先检测 whl 包，再检测 run 包 ===
if command -v mindie_motor_controller >/dev/null 2>&1 || command -v mindie_motor_coordinator >/dev/null 2>&1; then
    echo "Detected whl/pip layout (mindie_motor_* commands found on PATH)"
    MINDIE_IS_WHL_PACKAGE=1
elif [ -x "$MIES_INSTALL_PATH/bin/ms_controller" ] || [ -x "$MIES_INSTALL_PATH/bin/ms_coordinator" ]; then
    echo "Detected run layout (ms_* binaries found)"
    MINDIE_IS_WHL_PACKAGE=0
else
    echo "Error: Neither whl (mindie_motor_*) nor run (ms_*) layout detected."
    echo "Please check installation/PATH, or set MIES_INSTALL_PATH to a valid mindie-service root."
    echo "Hint: 若 mindie-service 安装失败（例如 om_adapter 多版本 pip 冲突），请先修复安装；并确认 PATH 上有 mindie_motor_*，或 \$MIES_INSTALL_PATH/bin 下有 ms_controller/ms_coordinator。"
    exit 1
fi
export MINDIE_IS_WHL_PACKAGE

if [ "${MINDIE_IS_WHL_PACKAGE:-1}" = "0" ]; then
    if [ ! -d "$MIES_INSTALL_PATH/bin" ]; then
        echo "Error: run layout requires $MIES_INSTALL_PATH/bin."
        exit 1
    fi
    # C++ 二进制在 $MIES_INSTALL_PATH/bin；om_adapter / node_manager 由 install 对 whl 做 pip install，
    # 可执行脚本在 pip 所用解释器的 scripts / user-base/bin 下。可显式覆盖：export MINDIE_PIP_PYTHON=python3.11
    mindie_resolve_pip_py_cmd() {
        if [ -n "${MINDIE_PIP_PYTHON:-}" ] && command -v "${MINDIE_PIP_PYTHON}" >/dev/null 2>&1; then
            echo "${MINDIE_PIP_PYTHON}"
            return 0
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            return 1
        fi
        _py_ver=$(python3 -c 'import sys; print(sys.version_info.major, sys.version_info.minor)' 2>/dev/null || true)
        _py_mm=$(echo "$_py_ver" | awk '{print $1"."$2}')
        case "$_py_mm" in
            3.11)
                command -v python3.11 >/dev/null 2>&1 && echo python3.11 && return 0
                ;;
            3.10)
                command -v python3.10 >/dev/null 2>&1 && echo python3.10 && return 0
                ;;
            3.9)
                command -v python3.9 >/dev/null 2>&1 && echo python3.9 && return 0
                ;;
        esac
        echo python3
        return 0
    }

    PATH="$MIES_INSTALL_PATH/bin:${PATH:-}"
    _mindie_py_cmd=$(mindie_resolve_pip_py_cmd) || _mindie_py_cmd=""
    if [ -n "${_mindie_py_cmd:-}" ]; then
        _py_scripts=$(${_mindie_py_cmd} -c 'import sysconfig; print(sysconfig.get_path("scripts"))' 2>/dev/null || true)
        if [ -n "${_py_scripts:-}" ] && [ -d "$_py_scripts" ]; then
            PATH="${_py_scripts}:${PATH}"
        fi
        _py_user_base=$(${_mindie_py_cmd} -m site --user-base 2>/dev/null || true)
        if [ -n "${_py_user_base:-}" ] && [ -d "$_py_user_base/bin" ]; then
            PATH="${_py_user_base}/bin:${PATH}"
        fi
    fi
    export PATH
    if ! command -v om_adapter >/dev/null 2>&1 || ! command -v node_manager >/dev/null 2>&1; then
        echo "Warning: om_adapter or node_manager not on PATH after run-layout env setup."
        echo "  MIES_INSTALL_PATH=$MIES_INSTALL_PATH"
        echo "  pip interpreter used for PATH: ${_mindie_py_cmd:-unknown}"
        echo "  Try: export MINDIE_PIP_PYTHON=python3.11"
        echo "  Or add the directory containing om_adapter to PATH (see: ${_mindie_py_cmd:-python3} -m site --user-base; ${_mindie_py_cmd:-python3} -c 'import sysconfig; print(sysconfig.get_path(\"scripts\"))')"
    fi
    export LD_LIBRARY_PATH="$MIES_INSTALL_PATH/lib:$MIES_INSTALL_PATH/lib/grpc:${LD_LIBRARY_PATH:-}"
fi

if [ -n "${MINDIE_LLM_HOME_PATH:-}" ] && [ -f "$MINDIE_LLM_HOME_PATH/set_env.sh" ]; then
    source "$MINDIE_LLM_HOME_PATH/set_env.sh"
elif [ -f "$MIES_INSTALL_PATH/scripts/set_env.sh" ]; then
    source "$MIES_INSTALL_PATH/scripts/set_env.sh"
else
    echo "Error: no set_env.sh found. Expected one of:"
    echo "  \$MINDIE_LLM_HOME_PATH/set_env.sh (MindIE-LLM)"
    echo "  \$MIES_INSTALL_PATH/scripts/set_env.sh (mindie-service install)"
    exit 1
fi
export MINDIE_LOG_TO_STDOUT=1
export MINDIE_LOG_LEVEL=info
export MINDIE_LOG_TO_FILE=1
# set_common_env支持用户修改环境变量覆盖原始MindIE环境变量
if ! type set_common_env >/dev/null 2>&1; then
    set_common_env() { :; }
fi
set_common_env

mindie_run_controller() {
    if [ "${MINDIE_IS_WHL_PACKAGE:-1}" = "0" ]; then
        "$MIES_INSTALL_PATH/bin/ms_controller" "$@"
    else
        mindie_motor_controller "$@"
    fi
}

mindie_run_coordinator() {
    if [ "${MINDIE_IS_WHL_PACKAGE:-1}" = "0" ]; then
        "$MIES_INSTALL_PATH/bin/ms_coordinator" "$@"
    else
        mindie_motor_coordinator "$@"
    fi
}

# DFX: copy failure must be logged and abort boot immediately
copy_or_fail() {
    if ! cp "$@"; then
        echo "Error: copy failed: cp $(printf '%q ' "$@")"
        exit 1
    fi
}

jemalloc_path=$(find /usr/lib /usr/lib64 -maxdepth 2 -type f -name "libjemalloc.so.2" 2>/dev/null | head -n 1)

if [[ -n "$jemalloc_path" ]]; then
    export LD_PRELOAD="${jemalloc_path}:${LD_PRELOAD}"
    echo "jemalloc found at: $jemalloc_path"
    echo "LD_PRELOAD is set successfully."
else
    echo "Warning: libjemalloc.so.2 not found under /usr"
    echo "Please make sure jemalloc is installed."
fi

CONFIG_DIR="$MIES_INSTALL_PATH/conf"

ELASTIC_SCALING_MOUNT_DEFAULT="$CONFIG_DIR/scaling/elastic_scaling.json"

elastic_scaling_sync_loop() {
    local src="${ELASTIC_SCALING_SRC:-$ELASTIC_SCALING_MOUNT_DEFAULT}"
    local dst="${ELASTIC_SCALING_DST:-$CONFIG_DIR/elastic_scaling.json}"
    if [ "$src" = "$dst" ]; then
        echo "elastic_scaling_sync: ELASTIC_SCALING_SRC equals destination, skip"
        return 0
    fi
    mkdir -p "$(dirname "$dst")"
    copy_elastic_scaling_once() {
        if [ ! -e "$src" ] && [ ! -L "$src" ]; then
            return 0
        fi
        if cat "$src" > "$dst.tmp" 2>/dev/null; then
            mv -f "$dst.tmp" "$dst"
            chmod 640 "$dst" 2>/dev/null || true
        fi
    }
    copy_elastic_scaling_once
    while true; do
        sleep 5
        copy_elastic_scaling_once
    done
}

cd "$MIES_INSTALL_PATH"

if [ $# -eq 0 ]; then
    PWD=$(cd "$(dirname "$0")"; pwd)
    BOOT_SCRIPT_DIR="$PWD"
    unset PWD
    exit_code=2
    if [ ! -n "$APP_TYPE" ]; then
        PYTHONUNBUFFERED=1 python3 "$BOOT_SCRIPT_DIR/get_group_id.py"
        exit_code=$?
    else
        PYTHONUNBUFFERED=1 python3 "$BOOT_SCRIPT_DIR/get_group_id.py"
        if [[ "$APP_TYPE" == *controller* ]]; then
            exit_code=1
        elif [[ "$APP_TYPE" == *coordinator* ]]; then
            exit_code=0
        else
            exit_code=2
        fi
    fi
    if [ $exit_code -eq 2 ]; then
        if [ -n "$CONFIG_FROM_CONFIGMAP_PATH" ]; then
            cp "$CONFIG_FROM_CONFIGMAP_PATH/config.json" "$CONFIG_DIR/config.json"
            cp "$CONFIG_FROM_CONFIGMAP_PATH/node_manager.json" "$CONFIG_DIR/node_manager.json"
            cp "$CONFIG_FROM_CONFIGMAP_PATH/http_client_ctl.json" "$CONFIG_DIR/http_client_ctl.json"
        fi
        export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common"
        python3 /mnt/configmap/file_utils.py "$CANN_INSTALL_PATH/ascend-toolkit/set_env.sh" --permission-mode 555 --max-size 104857600 || exit 1
        source "$CANN_INSTALL_PATH/ascend-toolkit/set_env.sh"
        python3 /mnt/configmap/file_utils.py "$CANN_INSTALL_PATH/nnal/atb/set_env.sh" --permission-mode 555 --max-size 104857600 || exit 1
        source "$CANN_INSTALL_PATH/nnal/atb/set_env.sh"
        export GRPC_POLL_STRATEGY=poll
        export MIES_CONTAINER_IP="$POD_IP"
        export MIES_CONTAINER_MANAGEMENT_IP="$POD_IP"

        HOME_HCCL_PATH=/home/hccl.json

        if [ -n "$MINDIE_LOG_CONFIG_PATH" ] && [ -n "$MODEL_NAME" ] && [ -n "$MODEL_ID" ]; then
            chmod 750 "$MINDIE_LOG_CONFIG_PATH"
            if [ ! -d "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID" ];then
                mkdir -p -m 750 "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID"
            fi
            if [ ! -d "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/mindie" ];then
                mkdir -p -m 750 "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/mindie"
            fi
            if [ ! -d "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_work_path" ];then
                mkdir -p -m 750 "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_work_path"
            fi
            if [ ! -d "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_cache_path" ];then
                mkdir -p -m 750 "$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_cache_path"
            fi
            export MINDIE_LOG_PATH="$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/mindie"
            export ASCEND_WORK_PATH="$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_work_path"
            export ASCEND_CACHE_PATH="$MINDIE_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/ascend_cache_path"
        fi

        if [ -n "$APP_TYPE" ]; then
            while true; do
                RANKTABLEFILE_TMP=/user/serverid/devindex/config/hccl/hccl.json;
                python3 /mnt/configmap/file_utils.py "$RANKTABLEFILE_TMP" --permission-mode 640 --max-size 104857600 || exit 1
                json_string=$(cat "$RANKTABLEFILE_TMP");
                echo "$json_string";
                status=$(echo "$json_string" | grep -o '\"status\":\"[^\"]*' | sed 's/\"status\":\"//');
                echo "$status";
                if [[ "$status" = "completed" ]]; then
                    echo "status is completed";
                    copy_or_fail /user/serverid/devindex/config/hccl/hccl.json "$HOME_HCCL_PATH"
                    chmod 640 "$HOME_HCCL_PATH"
                    break;
                fi;
                sleep 1;
            done;
        fi
        python3 /mnt/configmap/file_utils.py "$CONFIG_DIR/config.json" --permission-mode 640 --max-size 104857600 || exit 1
        if grep -q '"distDPServerEnabled": true' "$CONFIG_DIR/config.json"; then
            # 分布式
            export MINDIE_MS_GEN_SERVER_PORT=true
            chmod 750 "$CONFIG_DIR"
            touch "$CONFIG_DIR/npu_info.txt"
            chmod 640 "$CONFIG_DIR/npu_info.txt"
            npu-smi info -l > "$CONFIG_DIR/npu_info.txt"
            python3 /mnt/configmap/file_utils.py "$CONFIG_FROM_CONFIGMAP_PATH/gen_config_single_container.py" --permission-mode 550 || exit 1
            PYTHONUNBUFFERED=1 python3 "$CONFIG_FROM_CONFIGMAP_PATH/gen_config_single_container.py" "$CONFIG_DIR" multi-port
            exit_server_num=$?

            export RANKTABLEFILE="$HOME_HCCL_PATH"
            export RANK_TABLE_FILE="$HOME_HCCL_PATH"
            export MIES_RANKTABLEFILE="$HOME_HCCL_PATH"
            unset HCCL_OP_EXPANSION_MODE
            export ATB_WORKSPACE_MEM_ALLOC_ALG_TYPE=3
            export ATB_WORKSPACE_MEM_ALLOC_GLOBAL=1

            if [ "$ROLE" = "d" ]; then
                set_decode_env
                CURRENT_ROLE="decode"
            elif [ "$ROLE" = "p" ]; then
                set_prefill_env
                CURRENT_ROLE="prefill"
            fi
            export MALLOC_CONF="background_thread:true,tcache_nslots_small_max:20,tcache_nslots_small_min:5,narenas:4,dirty_decay_ms:5000,muzzy_decay_ms:5000"
            node_manager "$exit_server_num" "$CURRENT_ROLE" &
            pid=$!
            echo "pull up $CURRENT_ROLE instance, number: $exit_server_num"
        else
            chmod 750 "$CONFIG_DIR"
            python3 /mnt/configmap/file_utils.py "$BOOT_SCRIPT_DIR/update_mindie_server_config.py" --permission-mode 550 || exit 1
            PYTHONUNBUFFERED=1 python3 "$BOOT_SCRIPT_DIR/update_mindie_server_config.py" "$CONFIG_DIR/config.json"

            if [ -n "$DIST_PD_DISAGGREGATION" ]; then
                export RANKTABLEFILE="$HOME_HCCL_PATH"
                export RANK_TABLE_FILE="$HOME_HCCL_PATH"
                export MIES_RANKTABLEFILE="$HOME_HCCL_PATH"
                export ATB_WORKSPACE_MEM_ALLOC_ALG_TYPE=3
                export ATB_WORKSPACE_MEM_ALLOC_GLOBAL=1
                set_prefill_env
            fi
            export HCCL_RDMA_RETRY_CNT=7
            export HCCL_RDMA_TIMEOUT=18
            export HCCL_EXEC_TIMEOUT=60
            if [ ! -n "$APP_TYPE" ]; then
                if [ "${MINDIE_IS_WHL_PACKAGE:-1}" = "1" ]; then
                    /usr/local/bin/mindie_llm_server --config-file "$CONFIG_DIR/config.json" --expert-parallel true &
                else
                    "$MIES_INSTALL_PATH/bin/mindieservice_daemon" --config-file "$CONFIG_DIR/config.json" --expert-parallel true &
                fi
            else
                export MALLOC_CONF="background_thread:true,tcache_nslots_small_max:20,tcache_nslots_small_min:5,narenas:4,dirty_decay_ms:5000,muzzy_decay_ms:5000"
                node_manager &
            fi

            pid=$!
        fi
        wait $pid
        exit_code=$?
        if [ $exit_code -ne 0 ]; then
            echo "Error: mindie llm server exited with code $exit_code"
            exit 1
        fi
        echo "All processes finished successfully."
        exit 0
    fi

    if [ $exit_code -eq 1 ]; then
        if [ -n "$CONFIG_FROM_CONFIGMAP_PATH" ]; then
            cp "$CONFIG_FROM_CONFIGMAP_PATH/ms_controller.json" "$CONFIG_DIR/ms_controller.json"
            chmod 640 "$CONFIG_DIR/ms_controller.json"
            cp "$CONFIG_FROM_CONFIGMAP_PATH/http_client_ctl.json" "$CONFIG_DIR/http_client_ctl.json"
            chmod 640 "$CONFIG_DIR/http_client_ctl.json"
        fi
        cp "$GLOBAL_RANK_TABLE_FILE_PATH" "$MIES_INSTALL_PATH"
        export GLOBAL_RANK_TABLE_FILE_PATH="$MIES_INSTALL_PATH/global_ranktable.json"
        chmod 640 "$GLOBAL_RANK_TABLE_FILE_PATH"
        export MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH="$CONFIG_DIR/ms_controller.json"
        export MINDIE_LOG_PATH="$CONTROLLER_LOG_CONFIG_PATH"
        if [ -n "$CONTROLLER_LOG_CONFIG_PATH" ] && [ -n "$MODEL_NAME" ] && [ -n "$MODEL_ID" ]; then
            chmod 750 "$CONTROLLER_LOG_CONFIG_PATH"
            if [ ! -d "$CONTROLLER_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID" ];then
                mkdir -p -m 750 "$CONTROLLER_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID"
            fi
            export MINDIE_LOG_PATH="$CONTROLLER_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/mindie"
        fi
        set_controller_env
        elastic_scaling_sync_loop &
        om_adapter Controller &
        mindie_run_controller
    fi

    if [ $exit_code -eq 0 ]; then
        if [ -n "$CONFIG_FROM_CONFIGMAP_PATH" ]; then
            cp "$CONFIG_FROM_CONFIGMAP_PATH/ms_controller.json" "$CONFIG_DIR/ms_controller.json"
            chmod 640 "$CONFIG_DIR/ms_controller.json"
            cp "$CONFIG_FROM_CONFIGMAP_PATH/ms_coordinator.json" "$CONFIG_DIR/ms_coordinator.json"
            chmod 640 "$CONFIG_DIR/ms_coordinator.json"
            cp "$CONFIG_FROM_CONFIGMAP_PATH/http_client_ctl.json" "$CONFIG_DIR/http_client_ctl.json"
            chmod 640 "$CONFIG_DIR/http_client_ctl.json"
        fi
        export MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH="$CONFIG_DIR/ms_coordinator.json"
        export MINDIE_LOG_PATH="$COORDINATOR_LOG_CONFIG_PATH"
        if [ -n "$COORDINATOR_LOG_CONFIG_PATH" ] && [ -n "$MODEL_NAME" ] && [ -n "$MODEL_ID" ]; then
            chmod 750 "$COORDINATOR_LOG_CONFIG_PATH"
            if [ ! -d "$COORDINATOR_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID" ];then
                mkdir -p -m 750 "$COORDINATOR_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID"
            fi
            export MINDIE_LOG_PATH="$COORDINATOR_LOG_CONFIG_PATH/$MODEL_NAME/$MODEL_ID/mindie"
        fi
        set_coordinator_env
        if [ -n "$SERVICE_PROF_CONFIG_PATH" ]; then
            export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/Ascend/ascend-toolkit/latest/$(arch)-linux/devlib/linux/$(arch)"
            python3 /mnt/configmap/file_utils.py "$CANN_INSTALL_PATH/ascend-toolkit/set_env.sh" --permission-mode 555 || exit 1
            source $CANN_INSTALL_PATH/ascend-toolkit/set_env.sh
        fi
        om_adapter Coordinator &
        mindie_run_coordinator "$POD_IP" "$POD_IP"
    fi
elif [ $# -eq 1 ]; then
    if [[ "$1" == "single_container" ]]; then
        if [ -n "$CONFIG_FROM_CONFIG_FILE_PATH" ]; then
            cp -r "$CONFIG_FROM_CONFIG_FILE_PATH/..data/"* "$CONFIG_DIR/"
            chmod 640 "$CONFIG_DIR/"*
            if [ "$MINDIE_MS_GEN_SERVER_PORT" == "false" ]; then
                cp "$CONFIG_DIR/config1.json" "$CONFIG_DIR/config.json"
                chmod 640 "$CONFIG_DIR/config.json"
            fi
        fi

        export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$CANN_INSTALL_PATH/driver/lib64/driver:$CANN_INSTALL_PATH/driver/lib64/common"

        # update mindie server config
        chmod 750 "$CONFIG_DIR"
        touch "$CONFIG_DIR/npu_info.txt"
        chmod 640 "$CONFIG_DIR/npu_info.txt"
        npu-smi info -l > "$CONFIG_DIR/npu_info.txt"
        python3 /mnt/configmap/file_utils.py "$CONFIG_FROM_CONFIGMAP_PATH/gen_config_single_container.py" --permission-mode 550 || exit 1
        PYTHONUNBUFFERED=1 python3 "$CONFIG_FROM_CONFIGMAP_PATH/gen_config_single_container.py" "$CONFIG_DIR"
        exit_code=$?
        if [ $exit_code -eq 1 ]; then
            echo "Error: failed to update mindie server config."
            exit 1
        fi

        python3 /mnt/configmap/file_utils.py "$MIES_INSTALL_PATH/global_ranktable.json" --permission-mode 640 --max-size 104857600 || exit 1
        global_rank_table_content=$(cat "$MIES_INSTALL_PATH/global_ranktable.json")
        server_num=$(echo "$global_rank_table_content" | grep -o '"server_count": *[^,}]*' | head -n 1 | sed 's/"server_count": *\(.*\)/\1/')
        server_num=$(echo "$server_num" | tr -cd '[:digit:]')
        server_num=$(expr "$server_num" + 0)
        echo "server_num: $server_num"

        #pull up all server
        python3 /mnt/configmap/file_utils.py "$CANN_INSTALL_PATH/ascend-toolkit/set_env.sh" --permission-mode 555 || exit 1
        source "$CANN_INSTALL_PATH/ascend-toolkit/set_env.sh"
        python3 /mnt/configmap/file_utils.py "$CANN_INSTALL_PATH/nnal/atb/set_env.sh" --permission-mode 555 || exit 1
        source "$CANN_INSTALL_PATH/nnal/atb/set_env.sh"
        export GRPC_POLL_STRATEGY=poll

        for ((i=1; i<$server_num + 1; i++)); do
            export ATB_LLM_HCCL_ENABLE=1
            export PD_MODE=0
            export HCCL_OP_EXPANSION_MODE="AIV"
            export ATB_SHARE_MEMORY_NAME_SUFFIX="$i"
            export HCCL_RDMA_RETRY_CNT=7
            export HCCL_RDMA_TIMEOUT=18
            export HCCL_EXEC_TIMEOUT=60
            sleep 1
            if [ "${MINDIE_IS_WHL_PACKAGE:-1}" = "1" ]; then
                /usr/local/bin/mindie_llm_server --config-file "$CONFIG_DIR/config$i.json" --expert-parallel true &
            else
                "$MIES_INSTALL_PATH/bin/mindieservice_daemon" --config-file "$CONFIG_DIR/config$i.json" --expert-parallel true &
            fi
        done

        # pull up coordinator
        export MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH="$CONFIG_DIR/ms_coordinator.json"
        om_adapter Coordinator &
        mindie_run_coordinator "$POD_IP" &

        # pull up controller
        export GLOBAL_RANK_TABLE_FILE_PATH="$MIES_INSTALL_PATH/global_ranktable.json"
        export MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH="$CONFIG_DIR/ms_controller.json"
        om_adapter Controller &
        mindie_run_controller

    fi
fi
