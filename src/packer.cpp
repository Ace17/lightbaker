#include "scene.h"

#include <cmath>
#include <cstdio>

// set the uvLightmap coordinates
void packTriangles(Scene& s)
{
  // quick-and-dirty uniform packing
  auto count = (int)s.triangles.size();
  int cols = (int)ceil(sqrt(count));

  auto const step = 1.0f / cols;
  auto const size = step * 0.9f;

  int index = 0;

  for(auto& t : s.triangles)
  {
    int col = index % cols;
    int row = index / cols;

    auto margin = (step - size) * 0.5f;
    auto topLeft = Vec2 { col* step + margin, row* step + margin };
    auto botLeft = Vec2 { col* step + margin, row* step + size };
    auto topRight = Vec2 { col* step + size, row* step + margin };

    t.v[0].uvLightmap = topLeft;
    t.v[1].uvLightmap = botLeft;
    t.v[2].uvLightmap = topRight;

    ++index;
  }
}

