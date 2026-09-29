cmake_minimum_required(VERSION 3.5)
project(sdr-modem)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif ()

add_library(sdr_modemLib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api.pb-c.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp_worker.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/linked_list.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/queue.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sdr_worker.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app_config.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/bpsk_modem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dc_blocker.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fir_filter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/freq_offset.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gfsk_correlator.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gfsk_modem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/sdr_modem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/halfband_decim.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/halfband_interp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/psk_pm_modem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/plutosdr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/file_source.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/iio_lib.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/sdr_server_client.c
        )

find_package(PkgConfig REQUIRED)

find_path(LIQUID_INCLUDE_DIRS liquid/liquid.h)
find_library(LIQUID_LIBRARIES NAMES liquid)
include_directories(${LIQUID_INCLUDE_DIRS})
target_link_libraries(sdr_modemLib ${LIQUID_LIBRARIES})

pkg_check_modules(PC_LIBCONFIG REQUIRED libconfig)
include_directories(${PC_LIBCONFIG_INCLUDE_DIRS})
link_directories(${PC_LIBCONFIG_LIBRARY_DIRS})
target_link_libraries(sdr_modemLib ${PC_LIBCONFIG_LIBRARIES})

pkg_check_modules(PC_LIBPROTOBUFC REQUIRED libprotobuf-c)
include_directories(${PC_LIBPROTOBUFC_INCLUDE_DIRS})
link_directories(${PC_LIBPROTOBUFC_LIBRARY_DIRS})
target_link_libraries(sdr_modemLib ${PC_LIBPROTOBUFC_LIBRARIES})

if (APPLE)
    find_path(LIBIIO_INCLUDE_DIRS iio.h)
    include_directories(${LIBIIO_INCLUDE_DIRS})
    link_directories("/Library/Frameworks/iio.framework")
else ()
    pkg_check_modules(PC_LIBIIO REQUIRED libiio)
    include_directories(${PC_LIBIIO_INCLUDE_DIRS})
endif ()

find_package(Threads REQUIRED)
target_link_libraries(sdr_modemLib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sdr_modemLib m ${CMAKE_DL_LIBS})

find_package(ZLIB REQUIRED)
target_link_libraries(sdr_modemLib ZLIB::ZLIB)

add_executable(sdr_modem ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)
target_link_libraries(sdr_modem sdr_modemLib)

install(TARGETS sdr_modem DESTINATION /usr/bin/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/config.conf DESTINATION /etc/sdr-modem/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/sdr-modem.service DESTINATION /lib/systemd/system/)

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test/unity-2.5.2/src/)

# speed up test compilation.
add_library(sdr_modemTestLib
        ${CMAKE_CURRENT_SOURCE_DIR}/test/iio_lib_mock.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api.pb-c.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test/sdr_server_mock.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test/sdr_modem_client.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test/utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test/unity-2.5.2/src/unity.c
)

add_test(NAME test_dc_blocker COMMAND test_dc_blocker)
add_executable(test_dc_blocker ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dc_blocker.c)
target_link_libraries(test_dc_blocker sdr_modemLib sdr_modemTestLib)

add_test(NAME test_file_source COMMAND test_file_source)
add_executable(test_file_source ${CMAKE_CURRENT_SOURCE_DIR}/test/test_file_source.c)
target_link_libraries(test_file_source sdr_modemLib sdr_modemTestLib)

add_test(NAME test_freq_offset COMMAND test_freq_offset)
add_executable(test_freq_offset ${CMAKE_CURRENT_SOURCE_DIR}/test/test_freq_offset.c)
target_link_libraries(test_freq_offset sdr_modemLib sdr_modemTestLib)

add_test(NAME test_gfsk_modem COMMAND test_gfsk_modem)
add_executable(test_gfsk_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gfsk_modem.c)
target_link_libraries(test_gfsk_modem sdr_modemLib sdr_modemTestLib)

add_test(NAME test_bpsk_modem COMMAND test_bpsk_modem)
add_executable(test_bpsk_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/test_bpsk_modem.c)
target_link_libraries(test_bpsk_modem sdr_modemLib sdr_modemTestLib)

add_test(NAME test_psk_pm_modem COMMAND test_psk_pm_modem)
add_executable(test_psk_pm_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/test_psk_pm_modem.c)
target_link_libraries(test_psk_pm_modem sdr_modemLib sdr_modemTestLib)

add_test(NAME test_halfband_decim COMMAND test_halfband_decim)
add_executable(test_halfband_decim ${CMAKE_CURRENT_SOURCE_DIR}/test/test_halfband_decim.c)
target_link_libraries(test_halfband_decim sdr_modemLib sdr_modemTestLib)

add_test(NAME test_halfband_interp COMMAND test_halfband_interp)
add_executable(test_halfband_interp ${CMAKE_CURRENT_SOURCE_DIR}/test/test_halfband_interp.c)
target_link_libraries(test_halfband_interp sdr_modemLib sdr_modemTestLib)

add_test(NAME test_linked_list COMMAND test_linked_list)
add_executable(test_linked_list ${CMAKE_CURRENT_SOURCE_DIR}/test/test_linked_list.c)
target_link_libraries(test_linked_list sdr_modemLib sdr_modemTestLib)

add_test(NAME test_plutosdr COMMAND test_plutosdr)
add_executable(test_plutosdr ${CMAKE_CURRENT_SOURCE_DIR}/test/test_plutosdr.c)
target_link_libraries(test_plutosdr sdr_modemLib sdr_modemTestLib)

add_test(NAME test_queue COMMAND test_queue)
add_executable(test_queue ${CMAKE_CURRENT_SOURCE_DIR}/test/test_queue.c)
target_link_libraries(test_queue sdr_modemLib sdr_modemTestLib)

add_test(NAME test_app_config COMMAND test_app_config)
add_executable(test_app_config ${CMAKE_CURRENT_SOURCE_DIR}/test/test_app_config.c)
target_link_libraries(test_app_config sdr_modemLib sdr_modemTestLib)

add_test(NAME test_tcp_server COMMAND test_tcp_server)
add_executable(test_tcp_server ${CMAKE_CURRENT_SOURCE_DIR}/test/test_tcp_server.c)
target_link_libraries(test_tcp_server sdr_modemLib sdr_modemTestLib)

add_test(NAME test_cli COMMAND test_cli)
add_executable(test_cli ${CMAKE_CURRENT_SOURCE_DIR}/test/test_cli.c)
target_link_libraries(test_cli sdr_modemLib sdr_modemTestLib ${CMAKE_THREAD_LIBS_INIT})

file(GLOB TEST_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")

add_executable(perf_gfsk_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/perf_gfsk_modem.c)
target_link_libraries(perf_gfsk_modem sdr_modemLib sdr_modemTestLib)

add_executable(ber_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/ber_modem.c)
target_link_libraries(ber_modem sdr_modemLib)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_custom_target("coverage")
    add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/dsp/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/sdr/*.c.o)
endif ()

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "sdr-modem")
set(OS_CODENAME "$ENV{VERSION_CODENAME}")
if (OS_CODENAME STREQUAL "trixie" OR
    OS_CODENAME STREQUAL "noble")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libliquid1, libprotobuf-c1, libconfig11, zlib1g")
else ()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libliquid1, libprotobuf-c1, libconfig9, zlib1g")
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrey Rodionov <dernasherbrezon@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Modem based on software defined radios")
set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dernasherbrezon/sdr-modem")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_FILE_NAME "sdr-modem_${CPACK_DEBIAN_PACKAGE_VERSION}_${CUSTOM_ARCHITECTURE}.deb")
include(CPack)