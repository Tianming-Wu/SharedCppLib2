cmake_minimum_required(VERSION 3.10)

# if(POLICY CMP0111)
#     cmake_policy(SET CMP0111 NEW)
# endif()

project(SharedCppLib2
    VERSION 1.20.0
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

# 防止编码报错
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")

# 添加调试后缀
set(CMAKE_DEBUG_POSTFIX "d")

# 头文件目录
include_directories("include")

# 生成版本信息头文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/SharedCppLib2/version.hpp"
)

# 构建输出目录
# set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# 定义所有库
add_library(variant STATIC src/variant.cpp)
add_library(basic STATIC src/bytearray.cpp src/bytearray_base64.cpp src/ansiio.cpp src/stringlist.cpp src/basics.cpp)
add_library(logd STATIC src/logd.cpp)
add_library(logf STATIC src/logf.cpp)
add_library(sha256 STATIC src/sha256.cpp)
add_library(indexer STATIC src/indexer.cpp)
add_library(regexfilter STATIC src/regexfilter.cpp)
add_library(logt STATIC src/logt.cpp)
add_library(logc STATIC src/logc.cpp)
add_library(Base64 STATIC src/Base64.cpp)
add_library(platform STATIC src/platform.cpp)
# add_library(process STATIC src/process.cpp)
add_library(arguments STATIC src/arguments.cpp)

# 禁用 MSVC 的显式模板实例化警告
if(MSVC)
    target_compile_options(arguments PRIVATE /wd4661)
endif()

# 库依赖关系
target_link_libraries(logd PUBLIC variant)
target_link_libraries(logf PUBLIC logd)
target_link_libraries(logc PUBLIC logt)
target_link_libraries(arguments PUBLIC platform) # wstring 转换等功能需要平台相关支持

# 按需链接：bytearray_base64.cpp 会引用 Base64，但只有在用户代码调用相关函数时才会被链接
target_link_libraries(basic PRIVATE Base64)

# basic 库依赖关系
target_link_libraries(regexfilter PUBLIC basic)
target_link_libraries(logc PUBLIC basic)
target_link_libraries(sha256 PUBLIC basic)
target_link_libraries(arguments PUBLIC basic)

# 库列表
set(TARGET_LIST variant logd logf sha256 basic indexer regexfilter logt logc Base64 platform arguments)

# 安装目标
install(TARGETS ${TARGET_LIST}
    EXPORT SharedCppLib2Targets
    ARCHIVE DESTINATION ${INSTALL_LIBDIR}
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    RUNTIME DESTINATION ${INSTALL_BINDIR}
    INCLUDES DESTINATION ${INSTALL_INCLUDEDIR}
)

# 为每个目标设置导出属性
foreach(target_name ${TARGET_LIST})
    # 设置不同配置的库文件位置
    # set_property(TARGET ${target_name} PROPERTY 
    #     ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}")
    # set_property(TARGET ${target_name} PROPERTY 
    #     ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}")
    # set_property(TARGET ${target_name} PROPERTY 
    #     ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO}")
    # set_property(TARGET ${target_name} PROPERTY 
    #     ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL}")

    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

endforeach()


# 安装头文件
install(DIRECTORY include/
    DESTINATION ${INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# 安装生成的版本头文件
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/SharedCppLib2/version.hpp
    DESTINATION ${INSTALL_INCLUDEDIR}
)

# 安装导出文件
install(EXPORT SharedCppLib2Targets
    FILE SharedCppLib2Targets.cmake
    NAMESPACE SharedCppLib2::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 生成配置文件
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SharedCppLib2Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 安装配置文件
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SharedCppLib2Config.cmake  # 安装生成的配置文件
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
)

# 安装配置文件
# install(EXPORT SharedCppLib2Targets
#     FILE SharedCppLib2Config.cmake
#     NAMESPACE SharedCppLib2::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SharedCppLib2
# )

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
