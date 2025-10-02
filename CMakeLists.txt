cmake_minimum_required(VERSION 3.12)
project(timeseries-io VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置库类型（SHARED 或 STATIC）
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# 项目配置
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找源文件
file(GLOB TIMESERIES_IO_SOURCES
    "${PROJECT_SOURCE_DIR}/src/TimeSeriesReader.cpp"
    "${PROJECT_SOURCE_DIR}/src/TimeSeriesWriter.cpp"
    "${PROJECT_SOURCE_DIR}/src/TimeSeriesMultiReader.cpp"
)

# 创建库
add_library(timeseries-io ${TIMESERIES_IO_SOURCES})

# 设置库属性
set_target_properties(timeseries-io PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "timeseries-io"
)

# 包含目录（现代 CMake 方式）
target_include_directories(timeseries-io PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 编译特性
target_compile_features(timeseries-io PUBLIC cxx_std_17)

# 安装配置
install(TARGETS timeseries-io
    EXPORT timeseries-io-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include/timeseries-io
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# 生成和安装包配置文件
install(EXPORT timeseries-io-targets
    FILE timeseries-io-targets.cmake
    NAMESPACE timeseries-io::
    DESTINATION share/cmake/timeseries-io
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/timeseries-io-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/timeseries-io-config.cmake
    INSTALL_DESTINATION share/cmake/timeseries-io
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/timeseries-io-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/timeseries-io-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/timeseries-io-config-version.cmake
    DESTINATION share/cmake/timeseries-io
)

# 测试（可选）
# if(BUILD_TESTING)
#     enable_testing()
#     # 在这里添加测试目标
# endif()

# 打包支持
# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
