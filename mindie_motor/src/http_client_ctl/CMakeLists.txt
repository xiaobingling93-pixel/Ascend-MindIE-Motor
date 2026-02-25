project(MindIE-Utils-Http-Client)

set(PARENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(NOT DEFINED ENV{BUILD_MIES_3RDPARTY_INSTALL_DIR})
    message(FATAL_ERROR "BUILD_MIES_3RDPARTY_INSTALL_DIR is not set.")
endif()
set(BUILD_MIES_3RDPARTY_INSTALL_DIR $ENV{BUILD_MIES_3RDPARTY_INSTALL_DIR})

IF (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
ENDIF ()

IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    ADD_COMPILE_OPTIONS(-fPIE -fstack-protector-all -fPIC -std=c++17 -g -O0 --coverage -Werror=narrowing)
    ADD_LINK_OPTIONS(--coverage)
ELSE()
    ADD_COMPILE_OPTIONS(-fPIE -fstack-protector-all -fPIC -std=c++17 -O3 -ftrapv -D_FORTIFY_SOURCE=2
            -Wall -Wextra -Wfloat-equal -Werror=narrowing -fno-common)
    ADD_LINK_OPTIONS(-g -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
ENDIF()

SET(CMAKE_SKIP_RPATH TRUE)

include_directories(
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/boost/include
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/libboundscheck/include
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/nlohmann-json/include
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/openssl/include
)

link_directories(
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/libboundscheck/lib
    ${BUILD_MIES_3RDPARTY_INSTALL_DIR}/openssl/lib
)

include_directories(
    common
    log
    params
    http_client
)

file(GLOB util_http_client_ctl_files
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/params/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/http_client/*.cpp
    main.cpp
)

add_executable(http_client_ctl ${util_http_client_ctl_files})
target_link_libraries(http_client_ctl crypto ssl boundscheck pthread -ldl)

install(TARGETS http_client_ctl DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/http_client_ctl.json DESTINATION config)
