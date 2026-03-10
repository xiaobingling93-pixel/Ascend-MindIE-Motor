#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import time
import subprocess

from command_helper import CommandHelper
from utils import (
    print_to_screen,
    load_config,
    check_dir,
    get_time_stamp_ms,
    parent_dir
)


mies_repo_path = os.path.dirname(os.path.dirname(parent_dir))
ONE_SEC = 1
BOOL_TRUE = True


def has_output_file(command_helper_instance: CommandHelper, output_path: str, terminal_id: int = 0) -> bool:
    subprocess.Popen(
        f"tmux send-keys -t {command_helper_instance.session_name}:{terminal_id} "
        f"'ls {output_path} > {command_helper_instance.output[terminal_id]} 2>%1' C-m",
        shell=BOOL_TRUE)
    content = command_helper_instance.get_stdout(terminal_id)
    return "Ascend-mindie-service" in content


def wait_compile_complete(command_helper_instance: CommandHelper, output_path: str, terminal_id: int = 0):
    # clear output
    command_helper_instance.clear_output_buffer(terminal_id)
    retry_times = 240
    while retry_times > 0:
        time.sleep(30 * ONE_SEC)
        if has_output_file(command_helper_instance, output_path, terminal_id):
            print_to_screen(f'Compile completed.')
            break
        retry_times -= 1
    if retry_times == 0: 
        print_to_screen("Maximum retry times reached while waiting for compilation.")

    # wait all 'ls' command finished
    time.sleep(5 * ONE_SEC)


def compile_mies(command_helper_instance: CommandHelper, compile_config: dict):
    # compile source code
    print_to_screen("Start compile!")

    compiler_id = command_helper_instance.new_terminal()
    command_helper_instance.exec_command(
        compiler_id, f'docker start {compile_config["compile_contianer_name"]}', wait_time=5)
    command_helper_instance.exec_command(
        compiler_id, f'docker exec -it {compile_config["compile_contianer_name"]} bash', wait_time=5)
    command_helper_instance.wait_to_enter_container(compiler_id)

    # compile service
    # prepare dependency
    command_helper_instance.exec_command(compiler_id, f'cd {mies_repo_path}', wait_time=1)
    command_helper_instance.exec_command(compiler_id, f'mkdir -p third_party/install/MindIE-LLM', wait_time=1)
    unpackage_mindie_llm = f'bash {compile_config["mindie_llm_run_path"]} --extract=third_party/install/MindIE-LLM'
    command_helper_instance.exec_command(compiler_id, unpackage_mindie_llm, wait_time=1)
    command_helper_instance.exec_command(
        compiler_id,
        'cp -rf MindIE_Backends/MindIE_LLM_Backend/3rdparty/MindIE-LLM/include third_party/install/MindIE-LLM',
        wait_time=1)
    command_helper_instance.exec_command(
        compiler_id, 'cp -rf MindIE_Server/3rdparty/msServiceProfiler third_party/install', wait_time=1)
    command_helper_instance.exec_command(
        compiler_id, 'cp -f MindIE_Server/3rdparty/grpc/*.patch third_party/grpc_patch/', wait_time=1)
    # compile
    command_helper_instance.exec_command(
        compiler_id, 'export NO_CHECK_CERTIFICATE=true', wait_time=1)
    command_helper_instance.exec_command(
        compiler_id, 'export USE_GCC7=1', wait_time=1)
    command_helper_instance.exec_command(
        compiler_id, 'bash ci/build.sh 3rdparty-download', wait_time=10)
    command_helper_instance.exec_command(
        compiler_id, 'bash ci/build.sh 3rdparty', wait_time=150)
    command_helper_instance.exec_command(
        compiler_id, 'bash ci/build.sh mies', wait_time=150)

    output_path = f'{mies_repo_path}/output/aarch64'
    wait_compile_complete(command_helper_instance, output_path, compiler_id)

    # graceful exit
    command_helper_instance.exec_command(compiler_id, 'exit')

    print_to_screen("Compile task done!")


def deployment(
        command_helper_instance: CommandHelper, server_id: int, environment_config: dict):
    '''deploy latest compiled files
    '''
    mies_install_path = environment_config["mies_install_path"]

    mies_parent_dir, _ = os.path.split(mies_install_path)
    backup_folder_name = "mindie-service_bak"
    backup_folder_path = os.path.join(mies_parent_dir, backup_folder_name)

    ts = get_time_stamp_ms()
    backup_folder_name = f'{backup_folder_name}_{ts}'
    backup_folder_path = os.path.join(mies_parent_dir, backup_folder_name)

    # back up origin files
    command_helper_instance.exec_command(
        server_id, f'cp -r {mies_install_path} {backup_folder_path}', wait_time=5)
    print_to_screen(f"mindie-service folder has been back up as {backup_folder_path}")

    # copy compiled files to mies_install_path
    output_file_path = f'{mies_repo_path}/output/build/Ascend-mindie-service-1.0.0-linux-aarch64'
    if not check_dir(output_file_path):
        print_to_screen(f'Do not find directory {output_file_path}')
        raise ValueError(f'Do not find directory {output_file_path}')

    command_helper_instance.exec_command(
        server_id, f'cp -r {output_file_path} {mies_install_path}', wait_time=5)
    print_to_screen(f"copy directory: {output_file_path} to {mies_install_path}")


if __name__ == '__main__':
    # load config.json
    configs = load_config(f'{parent_dir}/config.json')
    compile_config = configs["compile_environment"]

    print_to_screen(compile_config)

    # create a tmux session
    command_helper_instance = CommandHelper('mysession')

    compile_mies(command_helper_instance, compile_config)