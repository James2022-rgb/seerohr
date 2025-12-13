cmake_minimum_required(VERSION 3.22)

project(
  seerohr
  VERSION 0.1.0.0
  LANGUAGES CXX
)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_DEAR_IMGUI "Use Dear ImGui." ON)
option(USE_RAYGUI "Use raygui." OFF)

# --------------------------------------------------------------------------------
# External Targets
#

set(THIRDPARTY_DIR "thirdparty")

#
# Dear ImGui
#

if(USE_DEAR_IMGUI)
  set(DEAR_IMGUI_DIR "${THIRDPARTY_DIR}/imgui")

  set(DEAR_IMGUI_SOURCES
    ${DEAR_IMGUI_DIR}/imgui.cpp
    ${DEAR_IMGUI_DIR}/imgui_demo.cpp
    ${DEAR_IMGUI_DIR}/imgui_draw.cpp
    ${DEAR_IMGUI_DIR}/imgui_tables.cpp
    ${DEAR_IMGUI_DIR}/imgui_widgets.cpp
    ${DEAR_IMGUI_DIR}/imconfig.h
    ${DEAR_IMGUI_DIR}/imgui.h
    ${DEAR_IMGUI_DIR}/imgui_internal.h
    ${DEAR_IMGUI_DIR}/imstb_rectpack.h
    ${DEAR_IMGUI_DIR}/imstb_textedit.h
    ${DEAR_IMGUI_DIR}/imstb_truetype.h
  )

  add_library(dear_imgui STATIC ${DEAR_IMGUI_SOURCES})
  target_include_directories(dear_imgui PUBLIC ${DEAR_IMGUI_DIR})
  set_target_properties(dear_imgui PROPERTIES FOLDER "external")
endif()

#
# raylib
#

set(RAYLIB_DIR "${THIRDPARTY_DIR}/raylib")
add_subdirectory(${RAYLIB_DIR})
set_target_properties(raylib PROPERTIES FOLDER "external")

#
# raylib-cpp
#

set(RAYLIB_CPP_DIR "${THIRDPARTY_DIR}/raylib-cpp")
add_subdirectory(${RAYLIB_CPP_DIR})
set_target_properties(raylib_cpp PROPERTIES FOLDER "external")

#
# raygui
#

if(USE_RAYGUI)
  set(RAYGUI_DIR "${THIRDPARTY_DIR}/raygui")
endif()

#
# rlImGui
#

if(USE_DEAR_IMGUI)
  set(RLIMGUI_DIR "${THIRDPARTY_DIR}/rlImGui")

  add_library(rlImGui STATIC
      ${RLIMGUI_DIR}/rlImGui.cpp
      ${RLIMGUI_DIR}/rlImGui.h
  )
  target_link_libraries(rlImGui dear_imgui raylib)
  target_include_directories(rlImGui PUBLIC ${RLIMGUI_DIR})
  set_target_properties(rlImGui PROPERTIES FOLDER "external")
endif()

# --------------------------------------------------------------------------------
# Dependencies
#

#
# mbase
#

set(MBASE_DIR "../mbase")

add_subdirectory(${MBASE_DIR} mbase)

# --------------------------------------------------------------------------------
# seerohr
#

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_executable(
  ${PROJECT_NAME}
  src/angle.cpp
  src/angle.h
  src/asset.cpp
  src/asset.h
  src/main.cpp
  src/raygui_integration.cpp
  src/raygui_widgets.cpp
  src/raygui_widgets.h
  src/raylib_widgets.cpp
  src/raylib_widgets.h
  src/tdc.cpp
  src/tdc.h
  src/text.cpp
  src/text.h
  src/widgets.cpp
  src/widgets.h
)

if(USE_RAYGUI)
  target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE
    ${RAYGUI_DIR}/src
  )
endif()

target_link_libraries(
  ${PROJECT_NAME}
  mbase
  raylib
  raylib_cpp
)

if(USE_DEAR_IMGUI)
  target_link_libraries(
    ${PROJECT_NAME}
    rlImGui
  )
endif()

if(USE_RAYGUI)
  target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_USE_RAYGUI=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_USE_RAYGUI=0)
endif()

# Web Configurations
if(${PLATFORM} STREQUAL "Web")
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsource-map=inline")
  
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html") # Tell Emscripten to build a ${PROJECT_NAME}.html file.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s FETCH -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1 -s EXPORTED_RUNTIME_METHODS=requestFullscreen")
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework IOKit")
  target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
  target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
