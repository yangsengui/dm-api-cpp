cmake_minimum_required(VERSION 3.15)
project(dm_api_cpp VERSION 1.2.0 LANGUAGES CXX)

add_library(dm_api_cpp INTERFACE)
add_library(dm::api_cpp ALIAS dm_api_cpp)

target_include_directories(dm_api_cpp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dm_api_cpp INTERFACE cxx_std_17)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dm_api_cppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/dm_api_cppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dm_api_cppConfig.cmake"
    INSTALL_DESTINATION "lib/cmake/dm_api_cpp"
)

install(FILES dm_api.hpp dm_api.h DESTINATION include)

install(TARGETS dm_api_cpp EXPORT dm_api_cppTargets)

install(
    EXPORT dm_api_cppTargets
    FILE dm_api_cppTargets.cmake
    NAMESPACE dm::
    DESTINATION lib/cmake/dm_api_cpp
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/dm_api_cppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/dm_api_cppConfigVersion.cmake"
    DESTINATION lib/cmake/dm_api_cpp
)
