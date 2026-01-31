set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HTTP_CLIENT_CTL_DIR ${MINDIE_SERVICE_ROOT}/mindie_motor/src/http_client_ctl)

file(GLOB ipc_llt_source_files
        ${CMAKE_CURRENT_SOURCE_DIR}/shared_memory/*.cpp
        ${MINDIE_MS_SRC_ROOT}/common/ipc/shared_memory/*.cpp
        ${MINDIE_MS_SRC_ROOT}/common/ipc/heartbeat/*.cpp
)

include_directories(
        # 源码
        ${MINDIE_MS_SRC_ROOT}/
        ${MINDIE_MS_SRC_ROOT}/common
        ${MINDIE_MS_SRC_ROOT}/common/ipc
        ${MINDIE_MS_SRC_ROOT}/common/ipc/shared_memory
        ${MINDIE_MS_SRC_ROOT}/common/ipc/heartbeat
)

file(GLOB COMMON_SOURCE_DIR
    ${MINDIE_MS_SRC_ROOT}/common/*.cpp
    ${MINDIE_MS_SRC_ROOT}/common/http_client/*.cpp
    ${MINDIE_MS_SRC_ROOT}/common/http_server/*.cpp
    ${HTTP_CLIENT_CTL_DIR}/log/*.cpp
    ${HTTP_CLIENT_CTL_DIR}/http_client/*.cpp
    ${HTTP_CLIENT_CTL_DIR}/common/*.cpp
    ${HTTP_CLIENT_CTL_DIR}/params/*.cpp
)

set(IPC_UT_FILES ${ipc_llt_source_files})
add_executable(mindie_ms_ipc_ut ${IPC_UT_FILES})

add_library(test_common SHARED ${COMMON_SOURCE_DIR})

target_link_libraries(mindie_ms_ipc_ut
    test_framework
    test_common
    gtest
    gmock
    crypto
    ssl
    dl
    pthread
    boundscheck
    rt
)

set(CMAKE_BUILD_TYPE Debug)