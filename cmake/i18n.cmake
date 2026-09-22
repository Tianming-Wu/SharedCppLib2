# ============================================================================
#  i18n helpers
#
#  随 SharedCppLib2 的 CMake 包一起安装；find_package(SharedCppLib2) 之后
#  即可直接使用本文件里的函数。
# ============================================================================

# ----------------------------------------------------------------------------
#  i18n_copy_lang(<target> <source_dir> [DEST_NAME <name>])
#
#  把 <source_dir> 目录整个复制到 <target> 的输出目录下，默认改名为 "lang"。
#  源目录叫什么都可以（translations/、i18n/、locales/ …）。
#
#  之所以固定成 "lang"：i18n::load(lang_code) 会按 "lang/<lang_code>.json"
#  去当前工作目录查找翻译文件。
#
#  示例：
#      add_executable(app main.cpp)
#      i18n_copy_lang(app "${CMAKE_CURRENT_SOURCE_DIR}/translations")
#      # 构建后：<output_dir>/lang/*.json
#
#      # 想换个目标目录名（默认就是 lang）：
#      i18n_copy_lang(app "${CMAKE_CURRENT_SOURCE_DIR}/i18n_src" DEST_NAME lang)
#
#  注意：
#   1. 这是 POST_BUILD 步骤，只在 <target> 重新构建时执行。只改了翻译文件、
#      却没触发 target 重建时，需要手动重新构建（或改动一下源文件）。
#   2. 复制到输出目录 ≠ 运行时一定找得到：i18n::load() 用的是**相对当前
#      工作目录**的路径。调试器的工作目录若不是输出目录，请把它设为
#      $<TARGET_FILE_DIR:app>，或自行把 lang/ 放到实际工作目录下。
# ----------------------------------------------------------------------------
function(i18n_copy_lang TARGET SOURCE_DIR)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR
            "i18n_copy_lang: first argument must be an existing target (got '${TARGET}')")
    endif()
    if(NOT IS_DIRECTORY "${SOURCE_DIR}")
        message(FATAL_ERROR
            "i18n_copy_lang: source directory does not exist: '${SOURCE_DIR}'")
    endif()

    cmake_parse_arguments(ARG "" "DEST_NAME" "" ${ARGN})
    if(NOT ARG_DEST_NAME)
        set(ARG_DEST_NAME "lang")
    endif()

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${SOURCE_DIR}"
                "$<TARGET_FILE_DIR:${TARGET}>/${ARG_DEST_NAME}"
        COMMENT "i18n: copying '${SOURCE_DIR}' -> ${ARG_DEST_NAME}/ next to ${TARGET}"
        VERBATIM
    )
endfunction()
