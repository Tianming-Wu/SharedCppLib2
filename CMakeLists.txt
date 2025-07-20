cmake_minimum_required(VERSION 3.10)

project(SharedCppLib2
    VERSION 1.10.1
    LANGUAGES CXX
)

# 基本配置
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 安装路径配置
include(GNUInstallDirs)
set(INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/SharedCppLib2)

# 添加调试后缀
set(CMAKE_DEBUG_POSTFIX "d")

# 头文件目录
include_directories("include")

# 构建输出目录
# set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# 定义所有库
add_library(variant STATIC variant.cpp)
add_library(basic STATIC bytearray.cpp ansiio.cpp stringlist.cpp basics.cpp)
add_library(logd STATIC logd.cpp)
add_library(logf STATIC logf.cpp)
add_library(sha256 STATIC sha256.cpp)

# 库依赖关系
target_link_libraries(logd PUBLIC variant)
target_link_libraries(logf PUBLIC logd)

# 安装目标
install(TARGETS variant logd logf sha256 basic
    EXPORT SharedCppLib2Targets
    ARCHIVE DESTINATION ${INSTALL_LIBDIR}
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    INCLUDES DESTINATION ${INSTALL_INCLUDEDIR}
)

target_include_directories(variant PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(basic PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(logd PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(logf PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(sha256 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 安装头文件
install(DIRECTORY include/
    DESTINATION ${INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
set(INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/SharedCppLib2)

# 安装导出文件
install(EXPORT SharedCppLib2Targets
    FILE SharedCppLib2Targets.cmake
    NAMESPACE SharedCppLib2::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 生成配置文件
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SharedCppLib2ConfigVersion.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 安装配置文件
install(EXPORT SharedCppLib2Targets
    FILE SharedCppLib2Config.cmake
    NAMESPACE SharedCppLib2::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 生成版本文件
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SharedCppLib2ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 安装版本文件
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SharedCppLib2ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)
