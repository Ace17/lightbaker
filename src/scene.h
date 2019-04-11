#pragma once

#include "vec.h"
#include <vector>

struct Vertex
{
  // read from input
  Vec3 pos;
  Vec3 N;
  Vec2 uvDiffuse;

  // computed
  Vec2 uvLightmap;
};

struct Triangle
{
  // read from input
  Vertex v[3];

  // computed
  Vec3 N;
};

struct Light
{
  Vec3 pos;
  Vec3 color;
  float falloff;
};

struct Scene
{
  std::vector<Triangle> triangles;
  std::vector<Light> lights;
};

