#pragma once

template<typename T>
struct Span
{
  T* data;
  size_t len;

  T & operator [] (int i) { return data[i]; }
  void operator ++ () { data++; len--; }
};

