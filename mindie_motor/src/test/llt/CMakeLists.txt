project(MindIE-MS-LLT)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(MINDIE_MS_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)
set(MINDIE_MS_TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../)

set(GOOGLT_TEST_DIR ${MINDIE_MS_TEST_ROOT}/open_source/googletest)
set(EMOCK_DIR ${MINDIE_MS_TEST_ROOT}/open_source/emock)
set(CPPFREEMOCK_DIR ${MINDIE_MS_TEST_ROOT}/open_source/emock)
set(CPP_STUB_DIR ${MINDIE_MS_TEST_ROOT}/open_source/cpp-stub)

include_directories(
    # 测试
    ${GOOGLT_TEST_DIR}/googletest/include
    ${GOOGLT_TEST_DIR}/googlemock/include
    # ${EMOCK_DIR}/include
    # ${CPPFREEMOCK_DIR}/include
    ${CPP_STUB_DIR}/src
)

add_subdirectory(${GOOGLT_TEST_DIR} gtest)
# add_subdirectory(${EMOCK_DIR} emock)

link_directories(
    ${MINDIE_MS_SRC_ROOT}/build/open_source/securec/lib
    ${MINDIE_MS_SRC_ROOT}/build/open_source/openssl
)

if("${BUILDING_STAGE}" STREQUAL "gcov")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
elseif("${BUILDING_STAGE}" STREQUAL "hitest_cov")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE hitestwrapper)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK hitestwrapper)
elseif("${BUILDING_STAGE}" STREQUAL "asan")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address -fsanitize=leak")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address -fsanitize=leak")
elseif("${BUILDING_STAGE}" STREQUAL "fuzz")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -fsanitize-coverage=trace-pc")
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined -fsanitize-coverage=trace-pc,trace-cmp -g -O0 -fPIC -fno-omit-frame-pointer -fsanitize-recover=address -fsanitize=leak")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fPIC -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -fsanitize-coverage=trace-pc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fPIC -fno-omit-frame-pointer")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -fsanitize-coverage=trace-pc,trace-cmp -g -O0 -fPIC -fno-omit-frame-pointer -fsanitize-recover=address -fsanitize=leak")
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdump-rtl-expand")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdump-rtl-expand")
elseif("${BUILDING_STAGE}" STREQUAL "tsan")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE -fsanitize=thread")
endif()

set(MINDIE_MS_SRCS 
    # ${MINDIE_MS_SRC_ROOT}/common
    ${MINDIE_MS_SRC_ROOT}/common/http_client
    # ${MINDIE_MS_SRC_ROOT}/infer_manager
    # ${MINDIE_MS_SRC_ROOT}/server/http_server
)

set(SRCS_FILES 
    ${MINDIE_MS_SRC_ROOT}/common/Logger.h
)

include_directories(
    # 源码
    ${MINDIE_MS_SRC_ROOT}/
    ${MINDIE_MS_SRC_ROOT}/common
    ${MINDIE_MS_SRC_ROOT}/common/http_client
    ${MINDIE_MS_SRC_ROOT}/deployer/infer_manager
    ${MINDIE_MS_SRC_ROOT}/deployer/server/http_server

    #三方
    ${MINDIE_MS_SRC_ROOT}/build/open_source/boost
    ${MINDIE_MS_SRC_ROOT}/build/open_source/securec/include
    ${MINDIE_MS_SRC_ROOT}/build/open_source/nlohmann/include
    ${MINDIE_MS_SRC_ROOT}/build/open_source/openssl/include
    ${MINDIE_MS_SRC_ROOT}/build/open_source
)

foreach(DIR ${MINDIE_MS_SRCS})
    aux_source_directory(${DIR} SRCS_FILES)
endforeach()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase SRCS_FILES)

list(REMOVE_ITEM SRCS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/testcase/Helper.cpp")

add_executable(mindie_ms_llt ${SRCS_FILES})
target_link_libraries(mindie_ms_llt gtest gmock)
target_link_libraries(mindie_ms_llt crypto ssl securec)

target_link_libraries(mindie_ms_llt dl pthread)
