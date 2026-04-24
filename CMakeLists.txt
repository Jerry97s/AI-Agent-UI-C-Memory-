cmake_minimum_required(VERSION 3.21)

project(AiAgentUiCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AiAgentUiCpp WIN32
  src/main.cpp
  src/http_agent_client.cpp
  src/http_agent_client.h
  src/strings.cpp
  src/strings.h
)

target_include_directories(AiAgentUiCpp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(AiAgentUiCpp PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

target_link_libraries(AiAgentUiCpp PRIVATE
  winhttp
  comctl32
)
