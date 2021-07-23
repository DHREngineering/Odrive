cmake_minimum_required(VERSION 3.15)
project(odrive)
set(DCMAKE_SH="CMAKE_SH-NOTFOUND")


set(CMAKE_CXX_STANDARD 14)

find_path(Jsoncpp_INCLUDE_DIR
  NAMES json/json.h
  PATH_SUFFIXES jsoncpp
  PATHS ${Jsoncpp_PKGCONF_INCLUDE_DIRS} # /usr/include/jsoncpp/json
)

add_executable(odrive main.cpp odrive.cpp)
target_link_libraries(odrive usb-1.0 jsoncpp)



