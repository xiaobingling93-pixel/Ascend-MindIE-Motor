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

SERVICE_ROOT_DIR=$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")
BUILD_MIES_3RDPARPT_ROOT_DIR="$SERVICE_ROOT_DIR"/third_party
ARCH=$(uname -i)
THIRD_PARTY_ZIP_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/zipped
SRC_ROOT_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/src
BUILD_ROOT_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/build
INSTALL_ROOT_DIR="$BUILD_MIES_3RDPARPT_INSTALL_DIR"
[ -z "$INSTALL_ROOT_DIR" ] && INSTALL_ROOT_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/install

# Number of processor when execute `make`, if not set, use all cpu
[ -z "$NUM_PROC" ] && NUM_PROC=$(nproc)

[ -z "$GLOBAL_ABI_VERSION" ] && GLOBAL_ABI_VERSION="0"
export GLOBAL_ABI_VERSION
echo "GLOBAL_ABI_VERSION: ${GLOBAL_ABI_VERSION}"

if [[ "$1" == "clean" ]]; then
    echo "Cleaning up the build directory..."
    rm -rf $BUILD_ROOT_DIR
    exit 0
fi

exist_msg=""

build_boost() {
    echo "Building Boost..."
    src_boost_dir="$SRC_ROOT_DIR"/boost
    build_boost_dir="$BUILD_ROOT_DIR"/boost
    install_boost_dir="$INSTALL_ROOT_DIR"/boost
    if [ -d "$install_boost_dir/lib" ]; then
        exist_msg="Found boost at $install_boost_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$build_boost_dir"
    mkdir -p "$install_boost_dir"

    cd "$src_boost_dir"
    chmod +x "./bootstrap.sh"
    chmod +x "./tools/build/src/engine/build.sh"

    ./bootstrap.sh
    ./b2 toolset=gcc \
        -j${NUM_PROC} \
        --disable-icu --with-thread --with-regex --with-log \
        --with-filesystem --with-date_time --with-chrono \
        cxxflags="-fstack-protector-strong -ftrapv \
        -D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION}" \
        linkflags="-Wl,-z,now -s" \
        threading=multi variant=release stage \
        --prefix="$install_boost_dir" install

    echo "Boost has been successfully built and installed to $install_boost_dir"
}

build_openssl() {
    echo "Building OpenSSL..."
    src_openssl_dir="$SRC_ROOT_DIR"/openssl
    build_openssl_dir="$BUILD_ROOT_DIR"/openssl
    install_openssl_dir="$INSTALL_ROOT_DIR"/openssl
    if [ -d "$install_openssl_dir/lib" ]; then
        exist_msg="Found openssl at $install_openssl_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$build_openssl_dir"
    mkdir -p "$install_openssl_dir"

    cd "$build_openssl_dir" || exit 1

    echo "Configuring OpenSSL build..."
    "$src_openssl_dir/config" --prefix="$install_openssl_dir" \
         --libdir="$install_openssl_dir/lib" \
         no-unit-test no-tests no-external-tests \
         CFLAGS="-fstack-protector-strong" \
         CXXFLAGS="-fstack-protector-strong -D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION}" \
         LDFLAGS="-Wl,-z,now -s"

    echo "Building OpenSSL..."
    make -j${NUM_PROC}

    make install
    echo "OpenSSL has been successfully built and installed in ${install_openssl_dir}"
}

build_spdlog() {
    src_spdlog_dir="$SRC_ROOT_DIR/spdlog"
    build_spdlog_dir="$BUILD_ROOT_DIR/spdlog"
    install_spdlog_dir="$INSTALL_ROOT_DIR/spdlog"
    if [ -d "$install_spdlog_dir/lib" ]; then
        exist_msg="Found spdlog at $install_spdlog_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$build_spdlog_dir"
    mkdir -p "$install_spdlog_dir"

    cd "$build_spdlog_dir"

    echo "Configuring spdlog build..."
    cmake "$src_spdlog_dir" -DCMAKE_INSTALL_PREFIX="$install_spdlog_dir" -DCMAKE_BUILD_TYPE=release \
            -DBUILD_SHARED_LIBS=OFF -DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION}
    echo "Building spdlog..."
    make -j${NUM_PROC}

    echo "Installing spdlog to $install_spdlog_dir..."
    make install
    echo "spdlog has been successfully built and installed to $install_spdlog_dir"
}

build_nlohmann_json() {
    src_nlohmann_json_dir="$SRC_ROOT_DIR/nlohmann-json"
    build_nlohmann_json_dir="$BUILD_ROOT_DIR/nlohmann-json"
    install_nlohmann_json_dir="$INSTALL_ROOT_DIR/nlohmann-json"
    if [ -d "$install_nlohmann_json_dir/include" ]; then
        exist_msg="Found nlohmann-json at $install_nlohmann_json_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$build_nlohmann_json_dir"
    mkdir -p "$install_nlohmann_json_dir"
    cd "$build_nlohmann_json_dir" || exit 1


    echo "Since nlohmann-json is a header-only lib, just copy it."
    echo "Copying nlohmann_json to install dir..."

    cmake -S "$src_nlohmann_json_dir" \
        -DCMAKE_BUILD_TYPE=release \
        -DJSON_BuildTests=OFF \
        -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION}" \
        -DCMAKE_INSTALL_PREFIX="$install_nlohmann_json_dir"

    make -j${NUM_PROC}
    make install
    echo "nlohmann_json has been successfully installed to $install_nlohmann_json_dir"
}

build_pybind11() {
    src_pybind11_dir="$SRC_ROOT_DIR/pybind11"
    build_pybind11_dir="$BUILD_ROOT_DIR/pybind11"
    install_pybind11_dir="$INSTALL_ROOT_DIR/pybind11"
    if [ -d "$install_pybind11_dir/include" ]; then
        exist_msg="Found pybind11 at $install_pybind11_dir, skip to build it.\n""$exist_msg"
        return
    fi
    if [ ! -d "$src_pybind11_dir" ]; then
        echo "Error: Source directory $src_pybind11_dir does not exist."
        exit 1
    fi

    mkdir -p "$build_pybind11_dir"
    cd "$build_pybind11_dir" || exit 1

    cmake "$src_pybind11_dir" \
        -DPYBIND11_NOPYTHON=ON -DPYBIND11_TEST=OFF -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION}" \
        -DCMAKE_INSTALL_PREFIX="$install_pybind11_dir"

    make -j${NUM_PROC}
    make install
    echo "pybind11 has been successfully installed to $install_pybind11_dir."
}

build_libboundscheck() {
    src_libboundscheck_dir="$SRC_ROOT_DIR/libboundscheck"
    build_libboundscheck_dir="$BUILD_ROOT_DIR/libboundscheck"
    install_libboundscheck_dir="$INSTALL_ROOT_DIR/libboundscheck"
    if [ -d "$install_libboundscheck_dir/lib" ]; then
        exist_msg="Found libboundscheck at $install_libboundscheck_dir, skip to build it.\n""$exist_msg"
        return
    fi
    if [ ! -d "$src_libboundscheck_dir" ]; then
        echo "Error: Source directory $src_libboundscheck_dir does not exist."
        exit 1
    fi

    mkdir -p "$install_libboundscheck_dir"
    cd "$src_libboundscheck_dir" || exit 1

    make -j${NUM_PROC}
    cp -rf "$src_libboundscheck_dir"/include "$install_libboundscheck_dir"
    cp -rf "$src_libboundscheck_dir"/lib "$install_libboundscheck_dir"

    echo "libboundscheck has been successfully installed to $install_libboundscheck_dir."
}

build_grpc() {
    echo "grpc is not support now, return"
    src_grpc_dir="$SRC_ROOT_DIR/grpc/grpc"
    build_grpc_dir="$BUILD_ROOT_DIR/grpc"
    install_grpc_dir="$INSTALL_ROOT_DIR/grpc"
    if [ -d "$install_grpc_dir/lib" ]; then
        exist_msg="Found grpc at $install_grpc_dir, skip to build it.\n""$exist_msg"
        return
    fi
    # patch grpc, we check it by find if `re2` exists.
    if [ ! -d "$src_grpc_dir"/third_party/re2 ]; then
        echo "patch grpc and its dependencies"
        bash "$SERVICE_ROOT_DIR"/build/third_party/patch_grpc_and_dependency.sh
    fi

    # grpc needs openssl
    install_openssl_dir="$INSTALL_ROOT_DIR/openssl"
    if [ ! -d "$install_openssl_dir/lib" ]; then
        echo "Grpc need openssl, make sure openssl is compiled and installed first."
    fi

    if [ ! -d "$src_grpc_dir" ]; then
        echo "Error: Source directory $src_grpc_dir does not exist."
        exit 1
    fi

    mkdir -p "$build_grpc_dir"
    cd "$build_grpc_dir" || exit 1

    cflags="-s"
    # For aarch64, add crc
    if [ "$ARCH" == "aarch64" ]; then
        cflags="-march=armv8-a+crc ""$cflags"
    fi

    cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_SSL_PROVIDER:STRING=package \
        -DOPENSSL_ROOT_DIR="$install_openssl_dir" \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_SKIP_INSTALL_RPATH=TRUE \
        -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=${GLOBAL_ABI_VERSION} -fstack-protector-all -ftrapv -s -D_FORTIFY_SOURCE=2 -O2" \
        -DCMAKE_C_FLAGS="$cflags" \
        -DgRPC_INSTALL=ON -DCMAKE_INSTALL_PREFIX="$install_grpc_dir" \
        "$src_grpc_dir"

    make -j${NUM_PROC}
    make install
    cp -r "$src_grpc_dir"/src "$install_grpc_dir"/include
    echo "grpc has been successfully installed to $install_grpc_dir."
}

build_prometheus_cpp() {
    src_prometheus_cpp_dir="$SRC_ROOT_DIR/prometheus-cpp"
    build_prometheus_cpp_dir="$BUILD_ROOT_DIR/prometheus-cpp"
    install_prometheus_cpp_dir="$INSTALL_ROOT_DIR/prometheus-cpp"
    if [ -d "$install_prometheus_cpp_dir/lib" ]; then
        exist_msg="Found prometheus-cpp at $install_prometheus_cpp_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$build_prometheus_cpp_dir"
    mkdir -p "$install_prometheus_cpp_dir"

    sed -i "1i add_link_options(-s)" "$src_prometheus_cpp_dir/CMakeLists.txt"
    sed -i "1i add_compile_options(-ftrapv)" "$src_prometheus_cpp_dir/CMakeLists.txt"
    sed -i "1i add_compile_options(-D_FORTIFY_SOURCE=2 -O2)" "$src_prometheus_cpp_dir/CMakeLists.txt"
    sed -i "1i add_compile_options(-fstack-protector-strong)" "$src_prometheus_cpp_dir/CMakeLists.txt"
    sed -i "1i add_link_options(-Wl,-z,relro,-z,now)" "$src_prometheus_cpp_dir/CMakeLists.txt"

    cmake "$src_prometheus_cpp_dir" \
        -B"$build_prometheus_cpp_dir" \
        -DCMAKE_BUILD_TYPE=release \
        -DBUILD_SHARED_LIBS=ON \
        -DENABLE_PUSH=OFF \
        -DENABLE_COMPRESSION=OFF \
        -DENABLE_TESTING=OFF \
        -DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI="${GLOBAL_ABI_VERSION}" \
        -DCMAKE_INSTALL_PREFIX="$install_prometheus_cpp_dir"

    make -C "$build_prometheus_cpp_dir" -j${NUM_PROC}
    make -C "$build_prometheus_cpp_dir" install
}

build_makeself() {
    src_makeself_dir="$SRC_ROOT_DIR/makeself"
    install_makeself_dir="$INSTALL_ROOT_DIR/makeself"
    if [ -d "$install_makeself_dir/makeself.sh" ]; then
        exist_msg="Found makeself at $install_makeself_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$install_makeself_dir"
    cp -rf "$src_makeself_dir"/makeself.sh "$install_makeself_dir"
    echo "makeself has been successfully installed to $install_makeself_dir."
}

build_googletest() {
    # Set directories for GoogleTest
    src_googletest_dir="$SRC_ROOT_DIR/googletest"
    build_googletest_dir="$BUILD_ROOT_DIR/googletest"
    install_googletest_dir="$INSTALL_ROOT_DIR/googletest"
    if [ -d "$install_googletest_dir/lib" ]; then
        exist_msg="Found googletest at $install_googletest_dir, skip to build it.\n""$exist_msg"
        return
    fi
    # Compile GoogleTest
    echo "Compiling GoogleTest..."
    mkdir -p ${build_googletest_dir}
    mkdir -p ${install_googletest_dir}

    cd ${build_googletest_dir}
    cmake ${src_googletest_dir} -DCMAKE_INSTALL_PREFIX=${install_googletest_dir} \
        -DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI="${GLOBAL_ABI_VERSION}" \
        -DCMAKE_BUILD_TYPE=Release
    make -j${NUM_PROC}
    make install

    echo "googletest has been successfully installed to $install_googletest_dir."
}

build_cpp_stub() {
    # Set directories cpp-stub
    src_cpp_stub_dir="$SRC_ROOT_DIR/cpp-stub"
    install_cpp_stub_dir="$INSTALL_ROOT_DIR/cpp-stub"
    if [ -d "$install_cpp_stub_dir/lib" ]; then
        exist_msg="Found cpp-stub at $install_cpp_stub_dir, skip to build it.\n""$exist_msg"
        return
    fi
    mkdir -p "$install_cpp_stub_dir"
    # For cpp-stub, just copy the header file.
    cp -rf "${src_cpp_stub_dir}"/src "$install_cpp_stub_dir"
    cp -rf "${src_cpp_stub_dir}"/src_linux "$install_cpp_stub_dir"

    echo "cpp-stub has been successfully installed to $install_cpp_stub_dir."
}

build_mockcpp() {
    src_mockcpp_dir="$SRC_ROOT_DIR/mockcpp"
    build_mockcpp_dir="$BUILD_ROOT_DIR/mockcpp"
    install_mockcpp_dir="$INSTALL_ROOT_DIR/mockcpp"
    if [ -d "$install_mockcpp_dir/lib" ]; then
        exist_msg="Found mockcpp at $install_mockcpp_dir, skip to build it.\n""$exist_msg"
        return
    fi
    echo "Compiling GoogleTest..."
    mkdir -p ${build_mockcpp_dir}
    mkdir -p ${install_mockcpp_dir}

    if [ -f "$SERVICE_ROOT_DIR"/tests/server/update.patch ]; then
        cd ${src_mockcpp_dir}
        patch -p1 -f < "$SERVICE_ROOT_DIR"/tests/server/update.patch || true
    fi
    cd ${build_mockcpp_dir}
    cmake ${src_mockcpp_dir} -DCMAKE_INSTALL_PREFIX=${install_mockcpp_dir} \
        -D_GLIBCXX_USE_CXX11_ABI="${GLOBAL_ABI_VERSION}" \
        -DCMAKE_BUILD_TYPE=Release
    make -j${NUM_PROC}
    make install

    echo "mockcpp has been successfully installed to $install_mockcpp_dir."
}

do_parallel=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --no-boost) flag_build_boost=false; echo "Skipping Boost"; shift ;;
        --no-openssl) flag_build_openssl=false; echo "Skipping OpenSSL"; shift ;;
        --no-spdlog) flag_build_spdlog=false; echo "Skipping spdlog"; shift ;;
        --no-nlohmann-json) flag_build_nlohmann_json=false; echo "Skipping nlohmann-json"; shift ;;
        --no-pybind11) flag_build_pybind11=false; echo "Skipping nlohmann-json"; shift ;;
        --no-libboundscheck) flag_build_libboundscheck=false; echo "Skipping libboundscheck"; shift ;;
        --no-grpc) flag_build_grpc=false; echo "Skipping grpc"; shift ;;
        --no-prometheus-cpp) flag_build_prometheus_cpp=false; echo "Skipping prometheus-cpp"; shift ;;
        --no-makeself) flag_build_makeself=false; echo "Skipping makeself"; shift ;;
        --with-googletest) flag_build_googletest=true; echo "Build googletest"; shift ;;
        --with-cpp-stub) flag_build_cpp_stub=true; echo "Build cpp-stub"; shift ;;
        --with-mockcpp) flag_build_mockcpp=true; echo "Build mockcpp"; shift ;;
        --only-test)
            flag_build_boost=false; flag_build_openssl=false; flag_build_spdlog=false;
            flag_build_nlohmann_json=false; flag_build_pybind11=false; flag_build_libboundscheck=false;
            flag_build_grpc=false; flag_build_prometheus_cpp=false;
            flag_build_googletest=true; flag_build_cpp_stub=true; flag_build_mockcpp=true;
            shift ;;
        --do-parallel) symbol_parallel="&"; echo "Compile parallel"; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

build_packages=()

if [[ "$flag_build_boost" != false ]]; then
    build_packages+=("build_boost $symbol_parallel")
fi

if [[ "$flag_build_openssl" != false ]]; then
    build_packages+=("build_openssl $symbol_parallel")
fi

if [[ "$flag_build_spdlog" != false ]]; then
    build_packages+=("build_spdlog $symbol_parallel")
fi

if [[ "$flag_build_nlohmann_json" != false ]]; then
    build_packages+=("build_nlohmann_json $symbol_parallel")
fi

if [[ "$flag_build_pybind11" != false ]]; then
    build_packages+=("build_pybind11 $symbol_parallel")
fi

if [[ "$flag_build_libboundscheck" != false ]]; then
    build_packages+=("build_libboundscheck $symbol_parallel")
fi

if [[ "$flag_build_grpc" != false ]]; then
    build_packages+=("build_grpc $symbol_parallel")
fi

if [[ "$flag_build_prometheus_cpp" != false ]]; then
    build_packages+=("build_prometheus_cpp $symbol_parallel")
fi

if [[ "$flag_build_makeself" != false ]]; then
    build_packages+=("build_makeself $symbol_parallel")
fi

if [[ "$flag_build_googletest" == true ]]; then
    build_packages+=("build_googletest $symbol_parallel")
fi

if [[ "$flag_build_cpp_stub" == true ]]; then
    build_packages+=("build_cpp_stub $symbol_parallel")
fi

if [[ "$flag_build_mockcpp" == true ]]; then
    build_packages+=("build_mockcpp $symbol_parallel")
fi

# build selected package
for cmd in "${build_packages[@]}"; do
    eval "$cmd"
done

if [ "$symbol_parallel" == "&" ]; then
    wait
fi
echo -e "$exist_msg"
echo "MindIE dependency libraries have been successfully built and installed to $INSTALL_ROOT_DIR"
echo "Success"
