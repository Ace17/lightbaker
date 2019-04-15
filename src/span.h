#pragma once

template<typename T>
struct Span
{
  T* ptr;
  size_t len;

  T & operator [] (int i) { return ptr[i]; }
  void operator ++ () { ptr++; len--; }
};

