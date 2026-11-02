// -*- mode: c++; coding: utf-8 -*-

// Linthesia

// Copyright (c) 2007 Nicholas Piegdon
// Adaptation to GNU/Linux by Oscar Aceña
// See COPYING for license information

#ifndef __STRING_UTIL_H
#define __STRING_UTIL_H

// Build strings without casting an ostream temporary to a derived type.
#include <sstream>
class LinthesiaStringBuilder {
public:
  template<class T> LinthesiaStringBuilder& operator<<(const T& value) {
    stream << value;
    return *this;
  }
  LinthesiaStringBuilder& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
    stream << manipulator;
    return *this;
  }
  LinthesiaStringBuilder& operator<<(std::ios_base& (*manipulator)(std::ios_base&)) {
    stream << manipulator;
    return *this;
  }
  std::string str() const { return stream.str(); }
private:
  std::ostringstream stream;
};
#ifndef STRING
#define STRING(v) ((LinthesiaStringBuilder() << v).str())
#endif

#include <string>
#include <vector>
#include <locale>
#include <algorithm>
#include <functional>
#include <iostream>

// string_type here can be things like std::string or std::wstring
template<class string_type>
const string_type StringLower(string_type s) {

  std::locale loc;
  
  const auto& facet = std::use_facet<std::ctype<typename string_type::value_type>>(loc);
  std::transform(s.begin(), s.end(), s.begin(),
                 [&facet](typename string_type::value_type c) { return facet.tolower(c); });
  
  return s;
}

// E here is usually wchar_t
template<class E, class T = std::char_traits<E>, class A = std::allocator<E> >
class Widen {

public:
  
  Widen(const std::locale& loc = std::locale()) : loc_(loc) {
    pCType_ = &std::use_facet<std::ctype<E> >(loc);
  }

  std::basic_string<E, T, A> operator() (const std::string& str) const {
    if (str.length() == 0) return std::basic_string<E, T, A>();

    typename std::basic_string<E, T, A>::size_type srcLen = str.length();
    const char* pSrcBeg = str.c_str();
    std::vector<E> tmp(srcLen);
    
    pCType_->widen(pSrcBeg, pSrcBeg + srcLen, &tmp[0]);
    return std::basic_string<E, T, A>(&tmp[0], srcLen);
  }

private:

  std::locale loc_;
  const std::ctype<E>* pCType_;
  
  // No copy-constructor or no assignment operator
  Widen(const Widen&);
  Widen& operator= (const Widen&);

};

#endif // __STRING_UTIL_H
