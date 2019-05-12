//===--- SimpleDisplay.h - Simple Type Display ------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//===----------------------------------------------------------------------===//
// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/05/05.
//===----------------------------------------------------------------------===//
//
//  This file defines the main customization point, simple_display, for
//  displaying values of a given type
//  encoding of (static) type information for use as a simple replacement
//  for run-time type information.
//
//===----------------------------------------------------------------------===//

#ifndef POLAR_BASIC_SIMPLE_DISPLAY_H
#define POLAR_BASIC_SIMPLE_DISPLAY_H

#include "polarphp/utils/RawOutStream.h"
#include <tuple>
#include <type_traits>

namespace polar::basic {

using polar::utils::RawOutStream;

template<typename T>
struct HasTrivialDisplay {
   static const bool value = false;
};

#define HAS_TRIVIAL_DISPLAY(Type)     \
   template<>                          \
   struct HasTrivialDisplay<Type> {    \
   static const bool value = true;   \
}

HAS_TRIVIAL_DISPLAY(unsigned char);
HAS_TRIVIAL_DISPLAY(signed char);
HAS_TRIVIAL_DISPLAY(char);
HAS_TRIVIAL_DISPLAY(short);
HAS_TRIVIAL_DISPLAY(unsigned short);
HAS_TRIVIAL_DISPLAY(int);
HAS_TRIVIAL_DISPLAY(unsigned int);
HAS_TRIVIAL_DISPLAY(long);
HAS_TRIVIAL_DISPLAY(unsigned long);
HAS_TRIVIAL_DISPLAY(long long);
HAS_TRIVIAL_DISPLAY(unsigned long long);
HAS_TRIVIAL_DISPLAY(float);
HAS_TRIVIAL_DISPLAY(double);
HAS_TRIVIAL_DISPLAY(bool);
HAS_TRIVIAL_DISPLAY(std::string);

#undef HAS_TRIVIAL_DISPLAY

template<typename T>
typename std::enable_if<HasTrivialDisplay<T>::value>::type
simple_display(RawOutStream &out, const T &value)
{
   out << value;
}

template<unsigned I, typename ...Types,
         typename std::enable_if<I == sizeof...(Types)>::type* = nullptr>
void simple_display_tuple(RawOutStream &out,
                          const std::tuple<Types...> &value);

template<unsigned I, typename ...Types,
         typename std::enable_if<I < sizeof...(Types)>::type* = nullptr>
void simple_display_tuple(RawOutStream &out,
         const std::tuple<Types...> &value)
{
   // Start or separator.
   if (I == 0) {
      out << "(";
   } else {
      out << ", ";
   }
   // Current element.
   simple_display(out, std::get<I>(value));
   // Print the remaining elements.
   simple_display_tuple<I+1>(out, value);
}

template<unsigned I, typename ...Types,
         typename std::enable_if<I == sizeof...(Types)>::type*>
void simple_display_tuple(RawOutStream &out,
                          const std::tuple<Types...> &)
{
   // Last element.
   out << ")";
}

template<typename ...Types>
void simple_display(RawOutStream &out,
                    const std::tuple<Types...> &value)
{
   simple_display_tuple<0>(out, value);
}

} // polar::basic

#endif // POLAR_BASIC_SIMPLE_DISPLAY_H