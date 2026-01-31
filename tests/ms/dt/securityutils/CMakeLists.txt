set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HTTP_CLIENT_CTL_DIR ${MINDIE_SERVICE_ROOT}/mindie_motor/src/http_client_ctl)

file(GLOB securityutils_llt_source_files
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${MINDIE_MS_SRC_ROOT}/common/securityutils/*.cpp
)

include_directories(
        # 源码
        ${MINDIE_MS_SRC_ROOT}/
        ${MINDIE_MS_SRC_ROOT}/common
        ${MINDIE_MS_SRC_ROOT}/common/securityutils
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

set(SECURITYUTILS_UT_FILES ${securityutils_llt_source_files})
add_executable(mindie_ms_securityutils_ut ${SECURITYUTILS_UT_FILES})

add_library(test_common SHARED ${COMMON_SOURCE_DIR})

target_link_libraries(mindie_ms_securityutils_ut
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
