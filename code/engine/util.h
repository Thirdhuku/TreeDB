/*
 * @file util.h
 * @brief 
 * 
 * @version 1.0
 * @date Mon May 13 17:51:20 2013
 * 
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#ifndef ENGINE_UTIL_H_
#define ENGINE_UTIL_H_

#include <sstream>

namespace util
{

template <typename D, typename S>
const D conv(const S& s) {
    std::stringstream ss;
    D d;
    ss << s;
    ss >> d;
    return d;
}

}

#endif  // ENGINE_UTIL_H_

