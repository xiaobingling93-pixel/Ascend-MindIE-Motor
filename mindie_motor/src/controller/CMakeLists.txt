project(MindIE-MS-Controller)

include_directories(
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/
    ${ROOT_PATH}/common/digs/
    ${ROOT_PATH}/common/http_server
    ${ROOT_PATH}/common/ipc/
    ${ROOT_PATH}/common/ipc/shared_memory/
    ${ROOT_PATH}/common/ipc/heartbeat/
    ${ROOT_PATH}/common/alarm_config/
    ${ROOT_PATH}/common/cluster_grpc/
    ${ROOT_PATH}/common/cluster_grpc/grpc_proto/
    ${ROOT_PATH}/common/leader_judge/
    ${ROOT_PATH}/common/leader_judge/etcd_proto/
    ${ROOT_PATH}/common/securityutils/
    ${ROOT_PATH}/common/utils/
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/json_file_loader/
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm_manager/alarm_listener/
    ${CMAKE_CURRENT_SOURCE_DIR}/constant/
    ${CMAKE_CURRENT_SOURCE_DIR}/ccae_reporter/
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_status/
    ${CMAKE_CURRENT_SOURCE_DIR}/process_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/request_handler/
    ${CMAKE_CURRENT_SOURCE_DIR}/probe_server/
    ${CMAKE_CURRENT_SOURCE_DIR}/status_updater/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/group_generator/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/rank_table_loader/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/role_switcher/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/role_manager_initializer/
    ${CMAKE_CURRENT_SOURCE_DIR}/fault_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_grpc/
    ${CMAKE_CURRENT_SOURCE_DIR}/leader_judge/
    ${CMAKE_CURRENT_SOURCE_DIR}/role_manager/
    ${CMAKE_CURRENT_SOURCE_DIR}/node_manager_sender/
    $ENV{ASCEND_HOME_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/
)

if (EXISTS $ENV{ASCEND_HOME_PATH}/include/msServiceProfiler/msServiceProfiler.h)
    add_definitions(-DWITH_PROF)
endif()

file(GLOB ms_controller_source_files
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/*.cpp
    ${ROOT_PATH}/common/http_server/*.cpp
    ${ROOT_PATH}/common/digs/*.cpp
    ${ROOT_PATH}/common/ipc/*.cpp   
    ${ROOT_PATH}/common/ipc/shared_memory/*.cpp
    ${ROOT_PATH}/common/ipc/heartbeat/*.cpp
    ${ROOT_PATH}/common/alarm_config/*.cpp
    ${ROOT_PATH}/common/cluster_grpc/*.cpp
    ${ROOT_PATH}/common/cluster_grpc/grpc_proto/*.cc
    ${ROOT_PATH}/common/leader_judge/*.cpp
    ${ROOT_PATH}/common/leader_judge/etcd_proto/*.cc
    ${ROOT_PATH}/common/securityutils/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json_file_loader/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm_manager/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/alarm_manager/alarm_listener/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/constant/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ccae_reporter/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_status/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/probe_server/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/process_manager/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/request_handler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/status_updater/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/group_generator/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/rank_table_loader/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/role_switcher/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_scheduler/role_manager_initializer/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fault_manager/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_manager/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster_grpc/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leader_judge/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/role_manager/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node_manager_sender/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
)

if (NOT DEFINED BUILD_MIES_3RDPARTY_INSTALL_DIR)
    message(WARNING "BUILD_MIES_3RDPARTY_INSTALL_DIR is not set, maybe some dependencies can't be found")
else()
    set(OPENSSL_ROOT_DIR "${BUILD_MIES_3RDPARTY_INSTALL_DIR}/openssl")
endif()

set(MS_CONTROLLER_SOURCE ${ms_controller_source_files})
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED CONFIG)
find_package(gRPC CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_GRPC_GRPCPP gRPC::grpc++)

add_executable(ms_controller ${MS_CONTROLLER_SOURCE})
target_link_libraries(ms_controller crypto ssl boundscheck pthread -ldl ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF})

install(TARGETS ms_controller DESTINATION bin)

install(FILES ${ROOT_PATH}/config/ms_controller.json DESTINATION config)
install(DIRECTORY ${ROOT_PATH}/config/machine_config DESTINATION config)
install(DIRECTORY ${ROOT_PATH}/config/model_config DESTINATION config)
