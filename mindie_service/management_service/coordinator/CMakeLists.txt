project(MindIE-MS-Coordinator)

add_definitions(-DBOOST_ASIO_NO_DEPRECATED)
add_definitions(-DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/connection_pool
    ${CMAKE_CURRENT_SOURCE_DIR}/common/data_structure
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_client
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_server
    ${CMAKE_CURRENT_SOURCE_DIR}/common/io_context_pool
    ${CMAKE_CURRENT_SOURCE_DIR}/common/communication
    ${CMAKE_CURRENT_SOURCE_DIR}/common/timer
    ${CMAKE_CURRENT_SOURCE_DIR}/common/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm
    ${CMAKE_CURRENT_SOURCE_DIR}/processor
    ${CMAKE_CURRENT_SOURCE_DIR}/configure
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/round_robin
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/cache_affinity
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/load_balance
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/node_store
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/request_monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/request_listener
    ${CMAKE_CURRENT_SOURCE_DIR}/request_repeater
    ${CMAKE_CURRENT_SOURCE_DIR}/controller_listener
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/framework
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/poolpolicy
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/reorderingpolicy
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/selectpolicy
    ${CMAKE_CURRENT_SOURCE_DIR}/request
    ${CMAKE_CURRENT_SOURCE_DIR}/resource
    ${CMAKE_CURRENT_SOURCE_DIR}/leader_judge/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/
    ${ROOT_PATH}/common/ipc/
    ${ROOT_PATH}/common/digs/
    ${ROOT_PATH}/common/ipc/shared_memory/
    ${ROOT_PATH}/common/ipc/heartbeat/
    ${ROOT_PATH}/common/alarm_config/
    ${ROOT_PATH}/common/cluster_grpc/
    ${ROOT_PATH}/common/cluster_grpc/grpc_proto/
    ${ROOT_PATH}/common/leader_judge/
    ${ROOT_PATH}/common/leader_judge/etcd_proto/
    ${ROOT_PATH}/common/securityutils
    ${CMAKE_CURRENT_SOURCE_DIR}
    $ENV{ASCEND_HOME_PATH}/include
)

if (EXISTS $ENV{ASCEND_HOME_PATH}/include/msServiceProfiler/msServiceProfiler.h)
    add_definitions(-DWITH_PROF)
endif()

file(GLOB ms_coordinator_source_files
    ${CMAKE_CURRENT_SOURCE_DIR}/common/connection_pool/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/data_structure/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_client/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_server/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/io_context_pool/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/communication/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/timer/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/utils/*.cpp
    ${ROOT_PATH}/common/ipc/*.cpp
    ${ROOT_PATH}/common/digs/*.cpp
    ${ROOT_PATH}/common/ipc/shared_memory/*.cpp
    ${ROOT_PATH}/common/ipc/heartbeat/*.cpp
    ${ROOT_PATH}/common/alarm_config/*.cpp
    ${ROOT_PATH}/common/cluster_grpc/*.cpp
    ${ROOT_PATH}/common/cluster_grpc/grpc_proto/*.cc
    ${ROOT_PATH}/common/leader_judge/*.cpp
    ${ROOT_PATH}/common/leader_judge/etcd_proto/*.cc
    ${ROOT_PATH}/common/securityutils/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/processor/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configure/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/proxy/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/node_store/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/round_robin/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/cache_affinity/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/default_scheduler/algorithm/load_balance/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_repeater/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_repeater/pd_repeater/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_repeater/single_repeater/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_repeater/exception_handler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_listener/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/controller_listener/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_monitor/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_monitor/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/framework/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/poolpolicy/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/reorderingpolicy/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digs_scheduler/selectpolicy/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leader_judge/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

if (NOT DEFINED BUILD_MIES_3RDPARTY_INSTALL_DIR)
    message(WARNING "BUILD_MIES_3RDPARTY_INSTALL_DIR is not set, maybe some dependencies can't be found")
else()
    set(OPENSSL_ROOT_DIR "${BUILD_MIES_3RDPARTY_INSTALL_DIR}/openssl")
endif()

set(MS_COORDINATOR_SOURCE ${ms_coordinator_source_files})

find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED CONFIG)
find_package(gRPC CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_GRPC_GRPCPP gRPC::grpc++)

add_executable(ms_coordinator ${MS_COORDINATOR_SOURCE})
target_link_libraries(ms_coordinator crypto ssl boundscheck rt pthread -ldl ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF})

install(TARGETS ms_coordinator DESTINATION bin)
install(FILES ${ROOT_PATH}/config/ms_coordinator.json DESTINATION config)
