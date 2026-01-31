set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HTTP_CLIENT_CTL_DIR ${MINDIE_SERVICE_ROOT}/mindie_motor/src/http_client_ctl)

file(GLOB mindie_service_utils_llt_source_files
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${HTTP_CLIENT_CTL_DIR}/log/*.cpp
)

include_directories(
        # 源码
        ${HTTP_CLIENT_CTL_DIR}/
        ${HTTP_CLIENT_CTL_DIR}/log
)

file(GLOB COMMON_SOURCE_DIR
        ${HTTP_CLIENT_CTL_DIR}/log/*.cpp
        ${HTTP_CLIENT_CTL_DIR}/common/*.cpp
)

set(MINDIE_SERVICE_UTILS_FILES ${mindie_service_utils_llt_source_files})
add_executable(mindie_service_utils_ut ${MINDIE_SERVICE_UTILS_FILES})

add_library(test_common SHARED ${COMMON_SOURCE_DIR})

target_link_libraries(mindie_service_utils_ut
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