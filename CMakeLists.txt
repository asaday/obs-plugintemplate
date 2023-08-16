cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)


add_library(${CMAKE_PROJECT_NAME} MODULE src/comment.c)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()


target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})


#target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "kernel32" "user32" "gdi32" "winspool" "comdlg32" "advapi32" "shell32" "ole32" "oleaut32" "uuid" "odbc32" "odbccp32")
#target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "kernel32" "user32")
#target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE -DWIN32_LEAN_AND_MEAN)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE UNICODE _UNICODE)
#target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

