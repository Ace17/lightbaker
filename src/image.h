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

inline float min(float a, float b)
{
  return a < b ? a : b;
}

inline float max(float a, float b)
{
  return a > b ? a : b;
}

