#pragma once

struct Pixel
{
  float r, g, b, a;
};

struct Image
{
  Pixel* pels;
  int width, height;
  int stride;
};

template<typename T>
inline T clamp(T val, T min, T max)
{
  if(val < min)
    return min;

  if(val > max)
    return max;

  return val;
}

template<typename T>
inline T min(T a, T b)
{
  return a < b ? a : b;
}

template<typename T>
inline T max(T a, T b)
{
  return a > b ? a : b;
}

