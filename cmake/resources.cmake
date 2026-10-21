# ============================================================================
#  resourced helpers
#
#  随 SharedCppLib2 的 CMake 包一起安装；find_package(SharedCppLib2) 之后
#  即可直接使用本文件里的函数。
#
#  这里做的是**构建期**那一半：把资源目录打成一个容器，并把资源名字生成成常量。
#  它不在 resourced 库里 —— 编译期需要知道的和加载期需要知道的不是一回事。
#  但两边共享格式：生成器链接 resourced 并直接调用 ResourcePack 把它写出来，
#  所以容器的布局只有一份实现，写端和读端不可能各自漂移。
#
#  两种用法：
#
#     scl2_add_resources(app DIR res/)                 打包并挂上
#     scl2_add_resources(app GENERATED gen/app)        只挂上已经生成好的
#
#  第二种是给交叉编译和 CI 留的口子：见下面的说明。
# ============================================================================

# ----------------------------------------------------------------------------
#  scl2_add_resources(<target> DIR <资源目录> [NAMESPACE <ns>] [STEM <name>]
#                                 [OUT_DIR <dir>] [LINK <库> ...])
#
#  scl2_add_resources(<target> GENERATED <已生成目录> [STEM <name>] [LINK <库> ...])
#
#  生成的头文件里，每个资源是一个编译期常量：
#
#      namespace <ns> {
#      inline constexpr std::string_view k_i18n_zh_CN_toml = "i18n/zh_CN.toml";
#      std::span<const std::byte> resources();
#      }
#
#  于是引用资源写成 <ns>::k_i18n_zh_CN_toml，打错名字是编译错误，而不是查不到。
#
#  运行时再把它挂上：
#
#      scl2::ResourceManager res;
#      res.mount("builtin", *scl2::ResourceView::parse(<ns>::resources()));
#
#  DIR 模式下，STEM 默认取目标名，于是 res/ 里生成 <target>.hpp / <target>.cpp，
#  NAMESPACE 默认也取目标名。GENERATED 模式下只找 <STEM>.cpp 并挂上，不跑任何工具。
#
#  注意：生成的代码只依赖 <cstddef> / <span> / <string_view>，不依赖本库，
#  所以那个 .cpp 里没有任何资源格式的逻辑，也不需要额外链接东西。
# ----------------------------------------------------------------------------
function(scl2_add_resources TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR
            "scl2_add_resources: '${TARGET}' is not an existing target")
    endif()

    cmake_parse_arguments(ARG "" "DIR;GENERATED;NAMESPACE;STEM;OUT_DIR" "LINK" ${ARGN})

    if(ARG_DIR AND ARG_GENERATED)
        message(FATAL_ERROR
            "scl2_add_resources: DIR and GENERATED are two different things; pass one")
    endif()
    if(NOT ARG_DIR AND NOT ARG_GENERATED)
        message(FATAL_ERROR
            "scl2_add_resources: pass DIR <resource-dir> or GENERATED <generated-dir>")
    endif()

    if(NOT ARG_STEM)
        set(ARG_STEM "${TARGET}")
    endif()

    if(ARG_LINK)
        target_link_libraries(${TARGET} PRIVATE ${ARG_LINK})
    endif()

    # ------------------------------------------------------------------
    #  GENERATED: attach files that were produced elsewhere. No tool is
    #  built and nothing is run.
    # ------------------------------------------------------------------
    if(ARG_GENERATED)
        set(_source "${ARG_GENERATED}/${ARG_STEM}.cpp")
        if(NOT EXISTS "${_source}")
            message(FATAL_ERROR
                "scl2_add_resources: '${_source}' does not exist. GENERATED expects "
                "<dir>/<stem>.cpp and <dir>/<stem>.hpp, produced by running resgen "
                "with: resgen <resource-dir> <dir>/<stem>.hpp <dir>/<stem>.cpp <namespace>")
        endif()

        target_sources(${TARGET} PRIVATE "${_source}")
        target_include_directories(${TARGET} PRIVATE "${ARG_GENERATED}")
        return()
    endif()

    # ------------------------------------------------------------------
    #  DIR: pack the directory now.
    # ------------------------------------------------------------------
    if(NOT IS_DIRECTORY "${ARG_DIR}")
        message(FATAL_ERROR
            "scl2_add_resources: DIR is not a directory: '${ARG_DIR}'")
    endif()
    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "${TARGET}")
    endif()
    if(NOT ARG_OUT_DIR)
        set(ARG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/scl2_resources/${ARG_STEM}")
    endif()

    # CMAKE_CURRENT_FUNCTION_LIST_DIR, not CMAKE_CURRENT_LIST_DIR: inside a function the latter
    # belongs to the caller's file, so the path would come out empty and the source argument would
    # silently disappear.
    if(CMAKE_VERSION VERSION_LESS "3.17.0")
        message(FATAL_ERROR "scl2_add_resources needs CMake 3.17 or newer")
    endif()

    # The tool is built once per build tree, from the source installed beside this file, and is
    # linked against the library so that the container layout has a single implementation.
    #
    # Set SCL2_RESGEN to an existing resgen to use that instead of building one — for a host-run
    # tool in a cross build, for instance.
    if(SCL2_RESGEN)
        set(_tool "${SCL2_RESGEN}")
    else()
        if(NOT TARGET scl2_resgen)
            add_executable(scl2_resgen "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/resgen_tool.cpp")
            target_link_libraries(scl2_resgen PRIVATE SharedCppLib2::resourced)
            set_target_properties(scl2_resgen PROPERTIES
                CXX_STANDARD 23
                CXX_STANDARD_REQUIRED ON)
            if(MSVC)
                target_compile_options(scl2_resgen PRIVATE /utf-8)
            endif()
        endif()
        set(_tool "scl2_resgen")
    endif()

    file(GLOB_RECURSE _scl2_res_files CONFIGURE_DEPENDS "${ARG_DIR}/*")

    set(_header "${ARG_OUT_DIR}/${ARG_STEM}.hpp")
    set(_source "${ARG_OUT_DIR}/${ARG_STEM}.cpp")

    add_custom_command(
        OUTPUT  "${_header}" "${_source}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUT_DIR}"
        COMMAND ${_tool} "${ARG_DIR}" "${_header}" "${_source}" "${ARG_NAMESPACE}"
        DEPENDS ${_scl2_res_files}
        COMMENT "resourced: packing '${ARG_DIR}' -> ${ARG_STEM}.cpp"
        VERBATIM)

    target_sources(${TARGET} PRIVATE "${_source}")
    target_include_directories(${TARGET} PRIVATE "${ARG_OUT_DIR}")
endfunction()

# ----------------------------------------------------------------------------
#  交叉编译
#
#  生成器是用**消费方的编译器**编的，所以交叉工具链下它会是跑不起来的目标平台二进制。
#  资源打包在交叉编译场景下不支持，也不打算支持：库本身没必要在那用 —— Android 有自己的
#  资源系统，Windows/Linux 原生编译很容易。
#
#  真需要时有两个现成出口：
#    1. 在目标平台上跑 resgen（安装目录的 bin/ 里有），把生成的两个文件拿回来，
#       然后用 GENERATED 形态挂上。代价是每次资源更新都要走一遍，无法自动化。
#    2. 用宿主编译器单独编一个 resgen，把路径给 SCL2_RESGEN。
#  不想走这两条路，就不要在交叉编译时使用资源打包。
#
#  另外 resgen --pack <out.pack> <dir> 可以把目录直接打成 .pack 文件，供运行时的
#  外部包来源（mount_pack_file）使用 —— 这条不经过编译器，交叉编译时也能用。
# ----------------------------------------------------------------------------
