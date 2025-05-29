#pragma once

#include <iostream>
#include <sstream>
#include <initializer_list>
#include <vector>

#include "variant.hpp"

namespace logd {

enum loglevel {
    verbose, debug, info, warn, error
};

/**
 * @brief Directly write to std::cout.
 * @param str The content.
 */
void logd(std::string str);

void logd_err(std::string str);

/**
 * @brief Directly write to std::cout.
 * @param str The content, $i (exp, $1) as keyword.
 * @param l Content replace list
 * 
 * usage example: `logd("Unknown operation: $1", {e.what()});`
 * 
 * Only one digit after the '$' will be read.
 * So, the function supports at most 9 variables ($1 ~ $9).
 * 
 * @warning If the digit can't be recgonized, or it exceeds the
 * number of variants in @c l , it will use an empty string
 * to replace it.
 */
void logd(std::string str, std::initializer_list<variant> l);

void logd_err(std::string str, std::initializer_list<variant> l);

std::string translate(std::string src, std::initializer_list<variant> l);

#define logd_dbg() logd("OOM: $1:$2 $3()", { __FILE__, __LINE__, __func__ })

};