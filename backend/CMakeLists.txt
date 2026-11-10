cmake_minimum_required(VERSION 3.16)
project(NFTOwnershipSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)   # benchmarks must be optimised
endif()

# ---- DAA core: no third-party dependencies --------------------------------
add_library(nft_core STATIC
  src/HashTable.cpp
  src/SortedArray.cpp
  src/NFTManager.cpp
  src/Benchmark.cpp)
target_include_directories(nft_core PUBLIC include)
if(MSVC)
  target_compile_options(nft_core PRIVATE /W3)
else()
  target_compile_options(nft_core PRIVATE -Wall -Wextra)
endif()

# Self-checks for the core (builds without Crow / nlohmann_json)
add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE nft_core)

# ---- REST server: needs Crow + nlohmann_json (installed with vcpkg) -----------
find_package(Crow CONFIG)
find_package(nlohmann_json CONFIG)

if(Crow_FOUND AND nlohmann_json_FOUND)
  add_executable(nft_server src/main.cpp)
  target_link_libraries(nft_server PRIVATE nft_core Crow::Crow nlohmann_json::nlohmann_json)
  if(WIN32)
    target_compile_definitions(nft_server PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(nft_server PRIVATE ws2_32 mswsock)
  endif()
else()
  message(WARNING "Crow and/or nlohmann_json not found - only 'core_test' will be built. "
                  "Install them with vcpkg and pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake (see README).")
endif()
