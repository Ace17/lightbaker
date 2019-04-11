#pragma once

struct Vec3
{
  float x, y, z;

  Vec3 operator * (float f) const { return { x* f, y* f, z* f }; }
  Vec3 operator + (Vec3 other) const { return { x + other.x, y + other.y, z + other.z }; }
  Vec3 operator - (Vec3 other) const { return { x - other.x, y - other.y, z - other.z }; }
};

inline float dotProduct(Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 crossProduct(Vec3 a, Vec3 b)
{
  Vec3 r;
  r.x = a.y * b.z - a.z * b.y;
  r.y = a.z * b.x - a.x * b.z;
  r.z = a.x * b.y - a.y * b.x;
  return r;
}

struct Vec2
{
  float x, y;

  Vec2 operator - (Vec2 other) const { return { x - other.x, y - other.y }; }
};

inline float dotProduct(Vec2 a, Vec2 b)
{
  return a.x * b.x + a.y * b.y;
}

