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
from collections import deque

from utils import print_to_screen, parent_dir


ONE_SEC = 1
BOOL_TRUE = True


class CommandHelper(object):
    '''Encapsulated commands of tmux to simulate multi-terminal command execution
    '''
    def __init__(self, session_name):
        self.session_name = session_name
        self.terminal_count = 0
        subprocess.Popen(["tmux", "new-session", "-d", "-s", f"{self.session_name}"], cwd=parent_dir)
        # wait session start successfully
        time.sleep(ONE_SEC)
        # create stdout buffer
        self.output = []

    def __del__(self):
        for file in self.output:
            os.remove(file)

    def new_terminal(self) -> int:
        ''' create a new terminal
        '''
        current_id = self.terminal_count
        self.terminal_count += 1
        if current_id != 0:
            subprocess.Popen(["tmux", "new-window", "-t", f"{self.session_name}:{current_id}"], cwd=parent_dir)
        new_output_file = f"{parent_dir}/output_{self.session_name}_{current_id}.txt"
        subprocess.Popen(["touch", new_output_file], cwd=parent_dir)
        self.output.append(new_output_file)
        time.sleep(ONE_SEC)
        return current_id

    def exec_command(self, terminal_id: int, command: str, need_output: bool = False,
                     wait_strs: list = None, wait_time: int = 0):
        '''execute command on a specific terminal
        Args:
            terminal_id : terminal id
            command: command to be executed on terminal
            need_output: need stdout
            wait_strs: all strings need to be waited
            wait_time: the duration of waiting for command execution
        '''
        command = self._add_output_command(command, terminal_id) if need_output else command
        subprocess.Popen(f"tmux send-keys -t {self.session_name}:{terminal_id} '{command}' C-m", shell=BOOL_TRUE)
        wait_strs = [] if wait_strs is None else wait_strs
        self.wait_str(terminal_id, wait_strs, wait_time)

    def get_stdout(self, terminal_id: int):
        '''read stdout from the file
        '''
        # wait redirect finish
        output_file = self.output[terminal_id]
        previous_mtime = os.path.getmtime(output_file)
        while True:
            time.sleep(0.1 * ONE_SEC)
            current_mtime = os.path.getmtime(output_file)
            if current_mtime == previous_mtime:
                break
            previous_mtime = current_mtime

        with open(output_file, 'r') as file:
            content = file.read()

        return content

    def is_in_container(self, terminal_id: int = 0):
        '''determine whether in a container or not
        '''
        subprocess.Popen(
            f"tmux send-keys -t {self.session_name}:{terminal_id} 'ls /.dockerenv > {self.output[terminal_id]}' C-m",
            shell=BOOL_TRUE)
        content = self.get_stdout(terminal_id)
        return content != ""

    def wait_to_enter_container(self, terminal_id: int = 0):
        '''determine enter a container successfully
        Args:
            terminal_id : terminal id
        '''
        current_time = time.time()
        timeout_time = current_time + 15
        while True:
            if time.time() >= timeout_time:
                print_to_screen(f'terminal-{terminal_id} enter docker failed')
                break
            time.sleep(3 * ONE_SEC)
            if self.is_in_container(terminal_id):
                print_to_screen(f'terminal-{terminal_id} is in docker')
                break
        # wait all 'ls' command finished
        time.sleep(5 * ONE_SEC)

    def wait_str(self, terminal_id: int, waited_strs: list, timeout: int = 5) -> bool:
        # if no waited_strs
        str_len = len(waited_strs)
        if str_len == 0:
            time.sleep(timeout)
            return True

        str_id = 0

        timeout_time = time.time() + timeout
        while True:
            time.sleep(10 * ONE_SEC)
            # timeout
            if time.time() >= timeout_time:
                return False
            # find last string in stdout
            with open(self.output[terminal_id], 'r') as file:
                last_line = deque(file, maxlen=1)
                last_string = str(last_line[0].strip()) if last_line else ""
            # match next string
            if waited_strs[str_id] in last_string:
                str_id += 1
            # already match all strings
            if str_id >= str_len:
                return True

    def clear_output_buffer(self, terminal_id: int) -> None:
        '''clear old output in output.txt
        '''
        with open(self.output[terminal_id], 'w') as f:
            pass

    def _add_output_command(self, command: str, terminal_id: int) -> str:
        '''redirect stdout to a file

        command: input command
        '''
        return f'{command} > {self.output[terminal_id]} 2>&1'


def kill_all_service(command_helper_instance, terminal_id):
    cmds = [
        "pkill -9 python",
        "pkill -9 python3",
        "pkill -9 mindie",
        "pkill -9 daemon",
        "pkill -9 benchmark",
        "pkill -9 ray",
        "pkill -9 worker",
        "pkill -9 text",
        "pkill -9 triton",
        "pkill -9 back",
        "pkill -9 test"
    ]
    for cmd in cmds:
        command_helper_instance.exec_command(terminal_id, f'{cmd}', wait_time=1)