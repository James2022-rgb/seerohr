cmake_minimum_required(VERSION 3.22)

project(
  mbase
  VERSION 0.1.0.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_library(
  ${PROJECT_NAME}
  include/mbase/container.h
  include/mbase/memory.h
  include/mbase/platform.h
  src/memory.cpp
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  include
)
