cmake_minimum_required(VERSION 2.8)
project(sdr-modem)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "-std=c99")
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif()

add_library(sdr_modemLib
		${CMAKE_CURRENT_SOURCE_DIR}/src/api_utils.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp_worker.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/linked_list.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/queue.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr_worker.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/server_config.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_utils.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/clock_recovery_mm.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dc_blocker.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/doppler.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fir_filter.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/frequency_modulator.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fsk_demod.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gaussian_taps.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/gfsk_mod.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/interp_fir_filter.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/lpf.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/lpf_taps.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/mmse_fir_interpolator.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/quadrature_demod.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/sig_source.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/math/fast_atan2f.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/iio_plugin.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/sdr_server_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/sgp4sdp4.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/sgp_in.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/sgp_math.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/sgp_obs.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/sgp_time.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sgpsdp/solar.c
		)

add_executable(sdr_modem ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)
target_link_libraries(sdr_modem sdr_modemLib)

install(TARGETS sdr_modem DESTINATION /usr/bin/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/config.conf DESTINATION /etc/sdr-modem/)

find_package(PkgConfig REQUIRED)

pkg_check_modules(PC_VOLK REQUIRED volk)
include_directories(${PC_VOLK_INCLUDE_DIRS})
target_link_libraries(sdr_modemLib ${PC_VOLK_LINK_LIBRARIES})

pkg_check_modules(PC_LIBCONFIG REQUIRED libconfig)
include_directories(${PC_LIBCONFIG_INCLUDE_DIRS})
target_link_libraries(sdr_modemLib ${PC_LIBCONFIG_LINK_LIBRARIES})

if (APPLE)
	find_path(LIBIIO_INCLUDE_DIRS iio.h)
	include_directories(${LIBIIO_INCLUDE_DIRS})

	# temporary
	add_compile_options("-F/Library/Frameworks/")
	link_libraries("-F/Library/Frameworks/")
	find_library(LIBIIO_LIBRARIES iio)
	target_link_libraries(sdr_modemLib ${LIBIIO_LIBRARIES})
else()
	pkg_check_modules(PC_LIBIIO REQUIRED libiio)
	include_directories(${PC_LIBIIO_INCLUDE_DIRS})
	# temporary
	target_link_libraries(sdr_modemLib ${PC_LIBIIO_LINK_LIBRARIES})
endif ()

find_package(Threads REQUIRED)
target_link_libraries(sdr_modemLib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sdr_modemLib m)

enable_testing()

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/test_*.c)
file(GLOB TEST_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")
file(GLOB PERF_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/perf_*.c)

file(GLOB AUX_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/*.c)
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "test_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "perf_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "mock_.*\\.c")

add_library(sdr_modemTestLib ${AUX_TEST_SOURCES})

pkg_check_modules(PC_CHECK REQUIRED check)
include_directories(${PC_CHECK_INCLUDE_DIRS})

foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_test(NAME ${curTestName} COMMAND ${curTestName} ${curTest})
	add_executable(${curTestName} ${curTest})
	target_link_libraries(${curTestName} sdr_modemLib sdr_modemTestLib ${PC_CHECK_LINK_LIBRARIES})
endforeach()

foreach(curPerfTest ${PERF_SOURCES})
	get_filename_component(curPerfTestName ${curPerfTest} NAME_WE)
	add_executable(${curPerfTestName} ${curPerfTest})
	target_link_libraries(${curPerfTestName} sdr_modemLib ${PC_CHECK_LINK_LIBRARIES})
endforeach()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/dsp/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/math/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/sdr/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_modemLib.dir/src/sgpsdp/*.c.o)
endif()