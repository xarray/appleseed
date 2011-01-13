
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef APPLESEED_FOUNDATION_UTILITY_MAKEVECTOR_H
#define APPLESEED_FOUNDATION_UTILITY_MAKEVECTOR_H

// Standard headers.
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <string>
#include <vector>

namespace foundation
{

//
// Build a std::vector<> out of a set of POD values.
//

std::vector<std::string> make_vector(const size_t n, const char* val, ...);

template <typename T>
std::vector<T> make_vector(const size_t n, const T val, ...);


//
// make_vector() functions implementation.
//

inline std::vector<std::string> make_vector(const size_t n, const char* val, ...)
{
    assert(n > 0);

    std::vector<std::string> vec;
    vec.push_back(val);

    va_list argptr;
    va_start(argptr, val);

    for (size_t i = 1; i < n; ++i)
        vec.push_back(va_arg(argptr, char*));

    va_end(argptr);

    return vec;
}

template <typename T>
std::vector<T> make_vector(const size_t n, const T val, ...)
{
    assert(n > 0);

    std::vector<T> vec;
    vec.push_back(val);

    va_list argptr;
    va_start(argptr, val);

    for (size_t i = 1; i < n; ++i)
        vec.push_back(va_arg(argptr, T));

    va_end(argptr);

    return vec;
}

}       // namespace foundation

#endif  // !APPLESEED_FOUNDATION_UTILITY_MAKEVECTOR_H
