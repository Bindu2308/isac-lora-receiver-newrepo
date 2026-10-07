cmake_minimum_required(VERSION 3.10)
project(isac_lora_gateway1 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)

# ---- PHY + protocol library (shared with gateway2 AND the receiver) ----
add_library(isac_common STATIC
    ${COMMON_DIR}/lora_phy/chirp.cpp
    ${COMMON_DIR}/lora_phy/crc.cpp
    ${COMMON_DIR}/lora_phy/whitening.cpp
    ${COMMON_DIR}/lora_phy/hamming.cpp
    ${COMMON_DIR}/lora_phy/interleaver.cpp
    ${COMMON_DIR}/lora_phy/lora_modulator.cpp
    ${COMMON_DIR}/lora_phy/lora_demodulator.cpp
    ${COMMON_DIR}/protocol/fragment.cpp
    ${COMMON_DIR}/protocol/observation.cpp
)
target_include_directories(isac_common PUBLIC ${COMMON_DIR})

# ---- Gateway pipeline library (shared verbatim with gateway2 -- this is
#      the fix for the Gateway1/Gateway2 divergence problem: it's the SAME
#      compiled object code, not a copy) ----
add_library(isac_gateway_pipeline STATIC
    ${COMMON_DIR}/gateway/gateway_runner.cpp
    ${COMMON_DIR}/gateway/detection/signal_detector.cpp
    ${COMMON_DIR}/gateway/synchronization/synchronizer.cpp
    ${COMMON_DIR}/gateway/network/gateway_client.cpp
    ${COMMON_DIR}/gateway/sdr/sdrplay_device.cpp
)
target_link_libraries(isac_gateway_pipeline PUBLIC isac_common pthread)

# ---- gateway1 executable: config only ----
add_executable(gateway1 src/main.cpp)
target_link_libraries(gateway1 PRIVATE isac_gateway_pipeline)

# ---- Optional: real SDRplay hardware support ----
# On the Pi, after installing SoapySDR + SoapySDRPlay3:
#   cmake -DWITH_SOAPYSDR=ON ..
option(WITH_SOAPYSDR "Build with real SDRplay hardware support via SoapySDR" OFF)
if(WITH_SOAPYSDR)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SOAPYSDR REQUIRED SoapySDR)
    target_compile_definitions(isac_gateway_pipeline PUBLIC ISAC_LORA_HAVE_SOAPYSDR=1)
    target_include_directories(isac_gateway_pipeline PUBLIC ${SOAPYSDR_INCLUDE_DIRS})
    target_link_libraries(isac_gateway_pipeline PUBLIC ${SOAPYSDR_LIBRARIES})
endif()
