cmake_minimum_required(VERSION 3.20)
project(RegDiff LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_library(RegDiffCore STATIC
  src/CommandLine.cpp
  "src/Regdiff/RegistryComparator.cpp"
  "src/Regdiff/RegistrySnapshot.cpp"
  "src/Regdiff/RegistrySnapshotter.cpp"
  "src/Regdiff/RegistryValueFormatter.cpp"
  "src/Regdiff/SnapshotJson.cpp"
  "src/Regdiff/TextReportWriter.cpp"
  "src/Regdiff/Win32RegistryAccess.cpp"
  "src/Regdiff/SnapshotOperations.cpp"  
  "src/UI/MainScreen.cpp"
  "src/Application.cpp"
  "src/ApplicationWindow.cpp"
  "src/CommandLine.cpp"
  "src/Platform/FileDialog.cpp"
)

target_include_directories(RegDiffCore PUBLIC include)

# Linked privately: no public header of RegDiffCore includes either library, so
# nothing that uses RegDiffCore has to know they are there.
target_link_libraries(RegDiffCore PRIVATE
  CLI11::CLI11
  nlohmann_json::nlohmann_json
  glfw
  OpenGL::GL
  imgui::imgui
  Ole32 # Windows COM support
)

# The executable is only a shim, so that everything it does can be tested.
add_executable(RegDiff src/Main.cpp)

target_link_libraries(RegDiff PRIVATE RegDiffCore)

enable_testing()

add_executable(RegDiffTests
  tests/CommandLineTests.cpp
  tests/RegistryComparatorTests.cpp
  tests/RegistrySnapshotTests.cpp
  tests/RegistrySnapshotterTests.cpp
  tests/SnapshotJsonTests.cpp
  tests/TextReportWriterTests.cpp
)

target_include_directories(RegDiffTests PRIVATE tests)

target_link_libraries(RegDiffTests PRIVATE
  RegDiffCore
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(RegDiffTests)
