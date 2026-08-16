cmake_minimum_required(VERSION 3.5)
project(sdr-modem)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/clock_recovery_mm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dc_blocker.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fir_filter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/frequency_modulator.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gmsk_modem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gaussian_taps.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/interp_fir_filter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/lpf.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/lpf_taps.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/mmse_fir_interpolator.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/quadrature_demod.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/sig_source.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/math/fast_atan2f.c
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

add_test(NAME test_clock_recovery_mm COMMAND test_clock_recovery_mm)
add_executable(test_clock_recovery_mm ${CMAKE_CURRENT_SOURCE_DIR}/test/test_clock_recovery_mm.c)
target_link_libraries(test_clock_recovery_mm sdr_modemLib sdr_modemTestLib)

add_test(NAME test_dc_blocker COMMAND test_dc_blocker)
add_executable(test_dc_blocker ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dc_blocker.c)
target_link_libraries(test_dc_blocker sdr_modemLib sdr_modemTestLib)

add_test(NAME test_dsp_worker COMMAND test_dsp_worker)
add_executable(test_dsp_worker ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dsp_worker.c)
target_link_libraries(test_dsp_worker sdr_modemLib sdr_modemTestLib)

add_test(NAME test_file_source COMMAND test_file_source)
add_executable(test_file_source ${CMAKE_CURRENT_SOURCE_DIR}/test/test_file_source.c)
target_link_libraries(test_file_source sdr_modemLib sdr_modemTestLib)

add_test(NAME test_frequency_modulator COMMAND test_frequency_modulator)
add_executable(test_frequency_modulator ${CMAKE_CURRENT_SOURCE_DIR}/test/test_frequency_modulator.c)
target_link_libraries(test_frequency_modulator sdr_modemLib sdr_modemTestLib)

add_test(NAME test_gmsk_modem COMMAND test_gmsk_modem)
add_executable(test_gmsk_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gmsk_modem.c)
target_link_libraries(test_gmsk_modem sdr_modemLib sdr_modemTestLib)

add_test(NAME test_gaussian_taps COMMAND test_gaussian_taps)
add_executable(test_gaussian_taps ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gaussian_taps.c)
target_link_libraries(test_gaussian_taps sdr_modemLib sdr_modemTestLib)

add_test(NAME test_interp_fir_filter COMMAND test_interp_fir_filter)
add_executable(test_interp_fir_filter ${CMAKE_CURRENT_SOURCE_DIR}/test/test_interp_fir_filter.c)
target_link_libraries(test_interp_fir_filter sdr_modemLib sdr_modemTestLib)

add_test(NAME test_linked_list COMMAND test_linked_list)
add_executable(test_linked_list ${CMAKE_CURRENT_SOURCE_DIR}/test/test_linked_list.c)
target_link_libraries(test_linked_list sdr_modemLib sdr_modemTestLib)

add_test(NAME test_lpf COMMAND test_lpf)
add_executable(test_lpf ${CMAKE_CURRENT_SOURCE_DIR}/test/test_lpf.c)
target_link_libraries(test_lpf sdr_modemLib sdr_modemTestLib)

add_test(NAME test_lpf_taps COMMAND test_lpf_taps)
add_executable(test_lpf_taps ${CMAKE_CURRENT_SOURCE_DIR}/test/test_lpf_taps.c)
target_link_libraries(test_lpf_taps sdr_modemLib sdr_modemTestLib)

add_test(NAME test_mmse_fir_interpolator COMMAND test_mmse_fir_interpolator)
add_executable(test_mmse_fir_interpolator ${CMAKE_CURRENT_SOURCE_DIR}/test/test_mmse_fir_interpolator.c)
target_link_libraries(test_mmse_fir_interpolator sdr_modemLib sdr_modemTestLib)

add_test(NAME test_plutosdr COMMAND test_plutosdr)
add_executable(test_plutosdr ${CMAKE_CURRENT_SOURCE_DIR}/test/test_plutosdr.c)
target_link_libraries(test_plutosdr sdr_modemLib sdr_modemTestLib)

add_test(NAME test_quadrature_demod COMMAND test_quadrature_demod)
add_executable(test_quadrature_demod ${CMAKE_CURRENT_SOURCE_DIR}/test/test_quadrature_demod.c)
target_link_libraries(test_quadrature_demod sdr_modemLib sdr_modemTestLib)

add_test(NAME test_queue COMMAND test_queue)
add_executable(test_queue ${CMAKE_CURRENT_SOURCE_DIR}/test/test_queue.c)
target_link_libraries(test_queue sdr_modemLib sdr_modemTestLib)

add_test(NAME test_app_config COMMAND test_app_config)
add_executable(test_app_config ${CMAKE_CURRENT_SOURCE_DIR}/test/test_app_config.c)
target_link_libraries(test_app_config sdr_modemLib sdr_modemTestLib)

add_test(NAME test_sig_source COMMAND test_sig_source)
add_executable(test_sig_source ${CMAKE_CURRENT_SOURCE_DIR}/test/test_sig_source.c)
target_link_libraries(test_sig_source sdr_modemLib sdr_modemTestLib)

add_test(NAME test_tcp_server COMMAND test_tcp_server)
add_executable(test_tcp_server ${CMAKE_CURRENT_SOURCE_DIR}/test/test_tcp_server.c)
target_link_libraries(test_tcp_server sdr_modemLib sdr_modemTestLib)

file(GLOB TEST_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")

add_executable(perf_gmsk_modem ${CMAKE_CURRENT_SOURCE_DIR}/test/perf_gmsk_modem.c)
target_link_libraries(perf_gmsk_modem sdr_modemLib sdr_modemTestLib)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_custom_target("coverage")
    add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/dsp/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/math/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/sdr/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/sgpsdp/*.c.o)
endif ()

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "sdr-modem")
set(OS_CODENAME "$ENV{VERSION_CODENAME}")
if (OS_CODENAME STREQUAL "trixie" OR
    OS_CODENAME STREQUAL "noble")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvolk-bin, libprotobuf-c1, libconfig11, zlib1g")
else ()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvolk2-bin, libprotobuf-c1, libconfig9, zlib1g")
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrey Rodionov <dernasherbrezon@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Modem based on software defined radios")
set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dernasherbrezon/sdr-modem")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_FILE_NAME "sdr-modem_${CPACK_DEBIAN_PACKAGE_VERSION}_${CUSTOM_ARCHITECTURE}.deb")
include(CPack)