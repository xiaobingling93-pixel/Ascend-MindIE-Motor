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

set -e

readonly CURRENT_PATH=$(cd "$(dirname "$0")"; pwd)
INSTALL_DIR="$BUILD_MINDIE_SERVICE_INSTALL_DIR"
[ -z "$INSTALL_DIR" ] && INSTALL_DIR="$CURRENT_PATH"/output
echo "Set INSTALL_DIR as ${INSTALL_DIR}"

# Number of processor when execute `make`, if not set, set as `max(1, nproc - 1)`.
[ -z "$NUM_PROC" ] && NUM_PROC=$(( ($(nproc) - 1) > 0 ? $(nproc) - 1 : 1 ))

build_type="Release"
while getopts "ds" opt; do
  case ${opt} in
    d)
      build_type="Debug"
      ;;
    s)
      build_stage="publish"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

function gen_recover_mindie_proto()
{
    PROTO_EXECUTE="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/protoc"
    grpc_cpp_plugin_execute="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/grpc_cpp_plugin"
    # The directory that generated .pb.h and .pb.cc in
    proto_install_dir="${CURRENT_PATH}/common/cluster_grpc/grpc_proto"
    # The directory that .proto file in
    proto_file_in_dir="${CURRENT_PATH}/common/cluster_grpc/grpc_proto"

    mkdir -p "${proto_install_dir}"
    recover_mindie_proto="${proto_file_in_dir}/$1"
    # Check this file to judge if .h and .cc file is generated
    dst_file_one="${proto_install_dir}/$2"
    # If file is not generated, or .proto file is updated, then generate the cxx and header file.
    if [ ! -f "$dst_file_one" ] || [ "$recover_mindie_proto" -nt "$dst_file_one" ]; then
      echo "Generate cxx and header file to grpc_proto from .proto file..."
      # Generate gRPC file
      "${PROTO_EXECUTE}" -I "${proto_file_in_dir}" --grpc_out="${proto_install_dir}" \
          --plugin=protoc-gen-grpc="${grpc_cpp_plugin_execute}" \
          "${recover_mindie_proto}"
      # Generate C++ file
      "${PROTO_EXECUTE}" -I "${proto_file_in_dir}" --cpp_out="${proto_install_dir}" \
          "${recover_mindie_proto}"
    else
      echo "Don't need to convert. $dst_file_one is up to date."
    fi

}

gen_recover_mindie_proto "recover_mindie.proto" "recover_mindie.pb.h"
gen_recover_mindie_proto "cluster_fault.proto" "cluster_fault.pb.h"

function gen_etcd_grpc_proto()
{
    PROTO_EXECUTE="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/protoc"
    grpc_cpp_plugin_execute="${BUILD_MIES_3RDPARTY_INSTALL_DIR}/grpc/bin/grpc_cpp_plugin"
    # The directory that generated .pb.h and .pb.cc in
    proto_install_dir="${CURRENT_PATH}/common/leader_judge/etcd_proto"
    # The directory that .proto file in
    proto_file_in_dir="${CURRENT_PATH}/common/leader_judge/etcd_proto"

    mkdir -p "${proto_install_dir}"
    proto_file_path="${proto_file_in_dir}/$1"
    # Check this file to judge if .h and .cc file is generated
    dst_file_one="${proto_install_dir}/$2"
    # If file is not generated, or .proto file is updated, then generate the cxx and header file.
    if [ ! -f "$dst_file_one" ] || [ "$proto_file_path" -nt "$dst_file_one" ]; then
      echo "Generate cxx and header file from ${proto_file_path}..."
      # Generate gRPC file
      "${PROTO_EXECUTE}" -I "${proto_file_in_dir}" --grpc_out="${proto_install_dir}" \
          --plugin=protoc-gen-grpc="${grpc_cpp_plugin_execute}" \
          "${proto_file_path}"
      # Generate C++ file
      "${PROTO_EXECUTE}" -I "${proto_file_in_dir}" --cpp_out="${proto_install_dir}" \
          "${proto_file_path}"
    else
      echo "Don't need to convert. $dst_file_one is up to date."
    fi
}

gen_etcd_grpc_proto "rpc.proto" "rpc.pb.h"
gen_etcd_grpc_proto "etcdserver.proto" "etcdserver.pb.h"
gen_etcd_grpc_proto "kv.proto" "kv.pb.h"

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=$build_type -DBUILDING_STAGE=$build_stage -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" ..

make install -j${NUM_PROC}
