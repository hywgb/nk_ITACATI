cmake_minimum_required(VERSION 3.16)
project(itacati-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(WITH_PJSIP_SYSTEM "Find PJSIP from system" ON)

add_executable(itacati-core
  src/main.cpp
)

target_include_directories(itacati-core PRIVATE src)

# Try to locate pjproject (PJSIP/PJSUA2)
if (WITH_PJSIP_SYSTEM)
  find_path(PJPROJECT_INCLUDE_DIRS NAMES pjsua2.hpp PATH_SUFFIXES include /usr/include /usr/local/include)
  find_library(PJ_LIB_pjsua2 NAMES pjsua2)
  find_library(PJ_LIB_pjsua NAMES pjsua)
  find_library(PJ_LIB_pjsip_ua NAMES pjsip_ua)
  find_library(PJ_LIB_pjsip_simple NAMES pjsip_simple)
  find_library(PJ_LIB_pjsip NAMES pjsip)
  find_library(PJ_LIB_pjmedia NAMES pjmedia)
  find_library(PJ_LIB_pjmedia_codec NAMES pjmedia-codec)
  find_library(PJ_LIB_pjmedia_videodev NAMES pjmedia-videodev)
  find_library(PJ_LIB_pjmedia_audiodev NAMES pjmedia-audiodev)
  find_library(PJ_LIB_pjnath NAMES pjnath)
  find_library(PJ_LIB_pjlib_util NAMES pjlib-util)
  find_library(PJ_LIB_pj NAMES pj)

  if (PJPROJECT_INCLUDE_DIRS AND PJ_LIB_pjsua2)
    target_include_directories(itacati-core PRIVATE ${PJPROJECT_INCLUDE_DIRS})
    target_link_libraries(itacati-core PRIVATE 
      ${PJ_LIB_pjsua2} ${PJ_LIB_pjsua} ${PJ_LIB_pjsip_ua} ${PJ_LIB_pjsip_simple} ${PJ_LIB_pjsip}
      ${PJ_LIB_pjmedia} ${PJ_LIB_pjmedia_codec} ${PJ_LIB_pjmedia_videodev} ${PJ_LIB_pjmedia_audiodev}
      ${PJ_LIB_pjnath} ${PJ_LIB_pjlib_util} ${PJ_LIB_pj}
    )
  else()
    message(WARNING "PJSIP not found; build may fail. Configure pjproject paths or build via Dockerfile.")
  endif()
endif()

if (MSVC)
  target_compile_definitions(itacati-core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()