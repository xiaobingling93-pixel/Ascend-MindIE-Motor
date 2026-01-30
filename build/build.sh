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

# Default option
RELEASE="release" # RELEASE can be 'release' or 'debug'
DOWNLOAD_MODULES_SELECTED=()
BUILD_MODULES_SELECTED=()
CLEAN_MODULES_SELECTED=()
PACKAGE_MINDIE_FLAG=false

# System config
ARCH=$(arch)

# User config
PROJECT_ROOT_DIR=$(dirname "$(dirname "$(realpath "$0")")")
CACHE_DIR="${PROJECT_ROOT_DIR}/build_cache"
OUTPUT_DIR="${PROJECT_ROOT_DIR}/output"
BUILD_SCRIPT_DIR="${PROJECT_ROOT_DIR}"/build
MODULES_DIR="${PROJECT_ROOT_DIR}"/modules

RELEASE_TMP_DIR="$OUTPUT_DIR"/cache
RELEASE_OUT_DIR="$OUTPUT_DIR"/$ARCH

[ -z "$BUILD_MIES_3RDPARTY_ROOT_DIR" ] && export BUILD_MIES_3RDPARTY_ROOT_DIR="$PROJECT_ROOT_DIR"/third_party
[ -z "$BUILD_MIES_3RDPARTY_INSTALL_DIR" ] && export BUILD_MIES_3RDPARTY_INSTALL_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install
[ -z "$BUILD_MIES_3RDPARTY_SRC_DIR" ] && export BUILD_MIES_3RDPARTY_SRC_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install

NUM_PROC=$(nproc)  # It will be overwrite by --parallel
# Default version
mindie_version="1.0.0"
export service_version="$mindie_version"
llm_version="$mindie_version"
sd_version="$mindie_version"

mindie_branch=master
service_branch="$mindie_branch"
llm_branch="$mindie_branch"
sd_branch="$mindie_branch"
git_depth=1        # Git clone depth

function print_config() {
    # Print choose parameter
    echo "Build Mode: $RELEASE"
    echo "Number of parallel threads: ${NUM_PROC}"
    echo "Modules to download: ${DOWNLOAD_MODULES_SELECTED[@]}"
    echo "Modules to build: ${BUILD_MODULES_SELECTED[@]}"
    echo "Modules to package: ${PACKAGE_COMPONENTS_SELECTED[@]}"
}

function parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
        -d | --download)
            shift
            DOWNLOAD_MODULES_SELECTED=()
            while [[ $# -gt 0 && ! "$1" =~ ^- ]]; do
                DOWNLOAD_MODULES_SELECTED+=("$1")
                shift
            done
            ;;
        -b | --build)
            shift
            BUILD_MODULES_SELECTED=()
            while [[ $# -gt 0 && ! "$1" =~ ^- ]]; do
                BUILD_MODULES_SELECTED+=($1)
                shift
            done
            ;;
        -p | --package)
            shift
            PACKAGE_MINDIE_FLAG=true
            ;;
        --release)
            RELEASE="release"
            shift
            ;;
        --parallel)
            NUM_PROC=$2
            shift 2
            export NUM_PROC
            ;;
        --debug)
            RELEASE="debug"
            shift
            ;;
        -a | --auto)
            shift
            echo "Found '-a' option at 'build/build.sh', build mindie automatically."
            DOWNLOAD_MODULES_SELECTED=("3rd" "llm")
            BUILD_MODULES_SELECTED=("3rd" "llm" "service")
            PACKAGE_MINDIE_FLAG=true
            ;;
        -h | --help)
            echo "Usage: build.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -a, --auto                    Automatically build mindie (downloads, builds, and packages 3rd, llm and service). Other options will be ignored."
            echo "  -d, --download [modules]      Download specified modules (3rd, llm)."
            echo "                                If no modules are specified, all are downloaded."
            echo "  -b, --build [modules]         Build specified modules (3rd, llm, service, server, ms, benchmark, client, atbmodels)."
            echo "  -p, --package                 Package mindie compiled output as a installable .run package."
            echo "      --release                 Set build mode to release. Without '--release' or '--debug', default to be '--release'."
            echo "      --debug                   Set build mode to debug. Without '--release' or '--debug', default to be '--release'."
            echo "  -h, --help                    Show this help message and exit."
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1, use '-h' or '--help' to get help."
            exit 1
            ;;
        esac
    done

    print_config
}

function download() {
    for comp in "${DOWNLOAD_MODULES_SELECTED[@]}"; do
        echo "Downloading $comp..."
        case "$comp" in
        "llm")
            download_mindie_repo MindIE-LLM "$mindie_branch" $git_depth
            ;;
        "3rd")
            bash "$BUILD_SCRIPT_DIR"/third_party/download_and_unzip.sh
            ;;
        *)
            echo "Unknown component $comp for downloading, skip it."
            ;;
        esac
    done
}

function download_mindie_repo() {
    repo="$1"
    branch="$2"
    git_depth="$3"
    [ -z "$repo" ] && echo "Repository is not given, exit!" && exit 2
    [ -z "$branch" ] && echo "Git branch is not given, use 'master' in default."
    [ -z "$git_depth" ] && echo "Git depth is not given, use '1' in default."

    url=https://gitcode.com/ascend/"$repo".git

    mkdir -p "$MODULES_DIR"
    cd "$MODULES_DIR"
    if [ ! -d "$repo" ]; then
        git clone --depth $git_depth -b $branch "$url"
    else
        echo "$MODULES_DIR/$repo exits, skip downloading $repo"
    fi
    cd -
}

function build() {
    if [[ ${#BUILD_MODULES_SELECTED[@]} -eq 0 ]]; then
        return
    fi
    for comp in "${BUILD_MODULES_SELECTED[@]}"; do
        echo "Building $comp..."
        # Choose specific module
        if [ "$RELEASE" = "debug" ]; then
            export DEBUG=true
        fi
        case $comp in
        service)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh service
            ;;
        ms)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh ms
            bash "$BUILD_SCRIPT_DIR"/service/build.sh http_client_ctl
            ;;
        benchmark)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh benchmark
            ;;
        client)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh client
            ;;
        llm)
            bash "$BUILD_SCRIPT_DIR"/modules/build_llm.sh
            ;;
        atbmodels)
            bash "$BUILD_SCRIPT_DIR"/modules/build_atbmodels.sh $RELEASE
            ;;
        sd)
            bash "$BUILD_SCRIPT_DIR"/modules/build_sd.sh
            ;;
        3rd)
            bash "$BUILD_SCRIPT_DIR"/third_party/build.sh
            ;;
        3rd-for-test)
            bash "$BUILD_SCRIPT_DIR"/third_party/build.sh --only-test
            ;;
        ms-test)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh ms-test
            ;;
        *)
            echo "Unknown module: $module, skip"
            ;;
        esac
    done
    echo "Build mindie completed successfully."
}

function clean() {
    if [[ ${#CLEAN_MODULES_SELECTED[@]} -eq 0 ]]; then
        return
    fi
    for comp in "${CLEAN_MODULES_SELECTED[@]}"; do
        echo "Cleaning $comp..."
        case $comp in
        service)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh service-clean
            ;;
        llm)
            bash "$BUILD_SCRIPT_DIR"/modules/build_llm.sh clean
            ;;
        sd)
            bash "$BUILD_SCRIPT_DIR"/modules/build_sd.sh clean
            ;;
        3rd)
            bash "$BUILD_SCRIPT_DIR"/service/build.sh 3rdparty-clean
            ;;
        *)
            echo "Unknown module: $module, skip"
            ;;
        esac
    done
    echo "Build mindie completed successfully."
}

function package() {
    if [[ "$PACKAGE_MINDIE_FLAG" == "true" ]]; then
        echo "Packaging mindie..."
        create_mindie_package
    fi
}

# Read fversion file
function parse_version_file() {
    VERSION_FILE="$PROJECT_ROOT_DIR/version.info"
    if [ ! -f "$VERSION_FILE" ]; then
        echo "Version file not set, using default version."
        return
    fi
    while IFS=':' read -r module_name value; do
        # Trim space
        module_name=$(echo "$module_name" | xargs)
        value=$(echo "$value" | xargs)

        case "$module_name" in
        Ascend-mindie) mindie_version="$value" ;;
        mindie-service) service_version="$value" ;;
        mindie-llm) llm_version="$value" ;;
        mindie-sd) sd_version="$value" ;;
        *) echo "Unknown module: $module_name" ;;
        esac
    done < "$VERSION_FILE"

    if [ -z "$mindie_version" ]; then
        echo "Error: Ascend-mindie version is required!"
        exit 1
    fi
    # If not set component version, using Ascend-mindie version
    if [ -z "$service_version" ]; then
        service_version="$mindie_version"
    fi
    if [ -z "$llm_version" ]; then
        llm_version="$mindie_version"
    fi
    if [ -z "$sd_version" ]; then
        sd_version="$mindie_version"
    fi
}

function print_versions() {
    echo "Ascend-mindie version: $mindie_version"
    echo "mindie-service version: $service_version"
    echo "mindie-llm version: $llm_version"
    echo "mindie-sd version: $sd_version"
    echo "Platform": $ARCH
}

function set_env() {
    # Auto detect ascend home path and source script to set env
    if [ -z "$ASCEND_HOME_PATH" ]; then
        if [ ! -f "/usr/local/Ascend/ascend-toolkit/set_env.sh" ]; then
            echo "Env 'ASCEND_HOME_PATH' is not set and can't find file '/usr/local/Ascend/ascend-toolkit/set_env.sh', please set 'ASCEND_HOME_PATH' manually or source the env file of ascend-toolkit."
            return
        else
            source /usr/local/Ascend/ascend-toolkit/set_env.sh
        fi
    elif [ -f "$ASCEND_HOME_PATH"/../set_env.sh ]; then
        source "$ASCEND_HOME_PATH"/../set_env.sh
    else
        echo "Warn: Can't find file $ASCEND_HOME_PATH/../set_env.sh, maybe result in error when build mindie-llm"
    fi
}

# Create mindie .run package, which contains llm, service etc.
function create_mindie_package() {
    MAKESELF_PATH="$BUILD_MIES_3RDPARTY_INSTALL_DIR"/makeself/makeself.sh
    RELEASE_CACHE_DIR="$PROJECT_ROOT_DIR/output/cache"
    LOG_PATH="/var/log/mindie_log/"
    LOG_NAME="mindie_install.log"

    rm -rf "${RELEASE_CACHE_DIR}" && mkdir -p ${RELEASE_CACHE_DIR}

    if [ ! -f "$MAKESELF_PATH" ]; then
        echo "$MAKESELF_PATH not exits, create .run package failed."
        exit 1
    fi
    MAKESELF_HEADER_PATH="$BUILD_SCRIPT_DIR"/common/makeself-header.sh
    if [ ! -f "$MAKESELF_HEADER_PATH" ]; then
        echo "$MAKESELF_HEADER_PATH not exists, create .run package failed."
        exit 1
    fi

    print_versions >${RELEASE_CACHE_DIR}/version.info
    mkdir -p ${RELEASE_CACHE_DIR}/scripts
    cp $BUILD_SCRIPT_DIR/mindie/uninstall.sh ${RELEASE_CACHE_DIR}/scripts
    cp $BUILD_SCRIPT_DIR/mindie/set_env.sh ${RELEASE_CACHE_DIR}
    cp $BUILD_SCRIPT_DIR/mindie/install.sh ${RELEASE_CACHE_DIR}
    cp $BUILD_SCRIPT_DIR/mindie/eula_en.txt ${RELEASE_CACHE_DIR}
    chmod 550 ${RELEASE_CACHE_DIR}/install.sh

    cd "$PROJECT_ROOT_DIR"/output/modules
    echo "Modules will package to mindie should be in dir '$PROJECT_ROOT_DIR/output/modules', it includes:\n"$(ls)
    if ls Ascend-mindie-service_*.run 1>/dev/null 2>&1; then
        cp -f Ascend-mindie-service_*.run ${RELEASE_CACHE_DIR}
    fi
    if ls Ascend-mindie-llm*.run 1>/dev/null 2>&1; then
        cp -f Ascend-mindie-llm*.run ${RELEASE_CACHE_DIR}
    fi
    if ls Ascend-mindie-sd*.tar.gz 1>/dev/null 2>&1; then
        cp -f Ascend-mindie-sd*.tar.gz ${RELEASE_CACHE_DIR}
    fi

    chmod +x ${RELEASE_CACHE_DIR}/*.run

    sed -i "s!VERSION=replace_version!VERSION=${mindie_version}!" ${RELEASE_CACHE_DIR}/install.sh
    sed -i "s!LOG_PATH=replace_log_path!LOG_PATH=${LOG_PATH}!" ${RELEASE_CACHE_DIR}/install.sh
    sed -i "s!LOG_NAME=replace_log_name!LOG_NAME=${LOG_NAME}!" ${RELEASE_CACHE_DIR}/install.sh
    sed -i "s!ARCH=replace_arch!ARCH=${ARCH}!" $RELEASE_CACHE_DIR/install.sh

    sed -i "s!VERSION=replace_version!VERSION=${mindie_version}!" ${RELEASE_CACHE_DIR}/scripts/uninstall.sh
    sed -i "s!LOG_PATH=replace_log_path!LOG_PATH=${LOG_PATH}!" ${RELEASE_CACHE_DIR}/scripts/uninstall.sh
    sed -i "s!LOG_NAME=replace_log_name!LOG_NAME=${LOG_NAME}!" ${RELEASE_CACHE_DIR}/scripts/uninstall.sh

    mkdir -p "$RELEASE_OUT_DIR"
    bash "$MAKESELF_PATH" --header "$MAKESELF_HEADER_PATH" --help-header $BUILD_SCRIPT_DIR/mindie/help.info \
        --gzip --complevel 4 --nomd5 --sha256 --chown ${RELEASE_CACHE_DIR} \
        "$RELEASE_OUT_DIR"/Ascend-mindie_${mindie_version}_linux-${ARCH}.run "Ascend-mindie" ./install.sh
    rm -rf "$RELEASE_CACHE_DIR"
    cd -
}

function main() {
    parse_version_file
    print_versions
    set_env

    parse_arguments "$@"
    download
    build
    package

    echo "Success"
}

main "$@"
