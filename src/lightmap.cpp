#include "vec.h"
#include "image.h"
#include "scene.h"

#include <cmath>

Vec3 normalize(Vec3 vec)
{
  auto const magnitude = dotProduct(vec, vec);
  return vec * (1.0 / sqrt(magnitude));
}

// return 'false' if the ray hit something
bool raycast(Triangle const& t, Vec3 rayStart, Vec3 rayDelta)
{
  auto const N = t.N;

  // coordinates along the normal axis
  auto t1 = dotProduct(N, rayStart);
  auto t2 = dotProduct(N, rayStart + rayDelta);
  auto plane = dotProduct(N, t.v[0].pos);

  if(t1 > plane && t2 > plane)
    return true; // plane was not crossed

  if(t1 < plane && t2 < plane)
    return true; // plane was not crossed

  // compute intersection point
  auto fraction = (plane - t1) / (t2 - t1);
  auto I = rayStart + rayDelta * fraction;

  // check if inside triangle
  for(int k = 0; k < 3; ++k)
  {
    auto a = t.v[(k + 0) % 3].pos;
    auto b = t.v[(k + 1) % 3].pos;
    auto outDir = crossProduct(b - a, N);

    if(dotProduct(I - a, outDir) >= 0)
      return true; // not in triangle
  }

  return false;
}

bool raycast(Scene const& s, Vec3 rayStart, Vec3 rayDelta)
{
  for(auto& t : s.triangles)
  {
    if(!raycast(t, rayStart, rayDelta))
      return false;
  }

  return true;
}

Pixel fragmentShader(Scene const& s, Vec3 pos, Vec3 N)
{
  Vec3 r {};

  // ambient light
  r = r + Vec3 { 0.1, 0.1, 0.1 };

  // avoid aliasing artifacts due to the light ray hitting the surface the fragment lies on
  auto const TOLERANCE = 0.01;

  for(auto& light : s.lights)
  {
    auto lightVector = light.pos - pos;

    // light ray is interrupted by an object
    if(!raycast(s, light.pos, lightVector * (-1 + TOLERANCE)))
      continue;

    auto dist = sqrt(dotProduct(lightVector, lightVector));
    auto cosTheta = dotProduct(lightVector * (1.0 / dist), N);
    float lightness = max(0, cosTheta) * 10.0f / (dist * dist);
    r = r + Vec3 { lightness* light.color.x,
                                          lightness* light.color.y,
                                          lightness* light.color.z };
  }

  return { r.x, r.y, r.z, 1 };
}

Vec3 barycentric(Vec2 p, Vec2 a, Vec2 b, Vec2 c)
{
  auto v0 = b - a;
  auto v1 = c - a;
  auto v2 = p - a;

  float d00 = dotProduct(v0, v0);
  float d01 = dotProduct(v0, v1);
  float d11 = dotProduct(v1, v1);
  float d20 = dotProduct(v2, v0);
  float d21 = dotProduct(v2, v1);
  float denom = d00 * d11 - d01 * d01;

  Vec3 r;
  r.y = (d11 * d20 - d01 * d21) / denom;
  r.z = (d00 * d21 - d01 * d20) / denom;
  r.x = 1.0f - r.y - r.z;
  return r;
}

struct Attributes
{
  Vec3 pos;
  Vec3 N;
};

void rasterizeTriangle(Image img, Scene const& scene, Vec2 v1, Attributes a1, Vec2 v2, Attributes a2, Vec2 v3, Attributes a3)
{
  auto const x1 = (int)(v1.x * img.width);
  auto const x2 = (int)(v2.x * img.width);
  auto const x3 = (int)(v3.x * img.width);

  auto const y1 = (int)(v1.y * img.height);
  auto const y2 = (int)(v2.y * img.height);
  auto const y3 = (int)(v3.y * img.height);

  auto const Dx12 = x1 - x2;
  auto const Dx23 = x2 - x3;
  auto const Dx31 = x3 - x1;

  auto const Dy12 = y1 - y2;
  auto const Dy23 = y2 - y3;
  auto const Dy31 = y3 - y1;

  // Bounding rectangle
  auto const minx = (int)clamp((int)min(min(x1, x2), x3), 0, img.width);
  auto const maxx = (int)clamp((int)max(max(x1, x2), x3), 0, img.width);
  auto const miny = (int)clamp((int)min(min(y1, y2), y3), 0, img.height);
  auto const maxy = (int)clamp((int)max(max(y1, y2), y3), 0, img.height);

  auto colorBuffer = img.pels;

  colorBuffer += miny * img.stride;

  // take into account filling convention
  int C1 = 0;
  int C2 = 0;
  int C3 = 0;

  if(Dy12 < 0 || (Dy12 == 0 && Dx12 > 0))
    C1++;

  if(Dy23 < 0 || (Dy23 == 0 && Dx23 > 0))
    C2++;

  if(Dy31 < 0 || (Dy31 == 0 && Dx31 > 0))
    C3++;

  for(int y = miny; y < maxy; y++)
  {
    for(int x = minx; x < maxx; x++)
    {
      auto const halfSpace12 = Dx12 * (y - y1) - Dy12 * (x - x1) + C1 > 0;
      auto const halfSpace23 = Dx23 * (y - y2) - Dy23 * (x - x2) + C2 > 0;
      auto const halfSpace31 = Dx31 * (y - y3) - Dy31 * (x - x3) + C3 > 0;

      if(halfSpace12 && halfSpace23 && halfSpace31)
      {
        auto p = Vec2 { (float)x / img.width, (float)y / img.height };
        auto bary = barycentric(p, v1, v2, v3);
        auto pos = a1.pos * bary.x + a2.pos * bary.y + a3.pos * bary.z;
        auto N = a1.N * bary.x + a2.N * bary.y + a3.N * bary.z;
        colorBuffer[x] = fragmentShader(scene, pos, N);
      }
    }

    colorBuffer += img.stride;
  }
}

void bakeLightmap(Scene& s, Image img)
{
  for(auto& t : s.triangles)
  {
    Attributes attr[3];

    for(int i = 0; i < 3; ++i)
    {
      attr[i].pos = t.v[i].pos;
      attr[i].N = t.v[i].N;
    }

    rasterizeTriangle(img,
                      s,
                      t.v[0].uvLightmap, attr[0],
                      t.v[1].uvLightmap, attr[1],
                      t.v[2].uvLightmap, attr[2]);
  }
}

void expandBorders(Image img)
{
  static auto const searchRange = 1;

  for(int row = 0; row < img.height; ++row)
  {
    for(int col = 0; col < img.width; ++col)
    {
      auto& pel = img.pels[col + row * img.stride];

      if(pel.a == 0)
      {
        for(int dy = -searchRange; dy <= searchRange; ++dy)
        {
          for(int dx = -searchRange; dx <= searchRange; ++dx)
          {
            auto x = clamp(col + dx, 0, img.width - 1);
            auto y = clamp(row + dy, 0, img.height - 1);
            auto nb = img.pels[x + y * img.stride];

            if(nb.a == 1.0)
            {
              pel = nb;
              pel.a = 0.99;
              goto ok;
            }
          }
        }

        ok:
        int a;
      }
    }
  }

  // threshold alpha
  for(int row = 0; row < img.height; ++row)
  {
    for(int col = 0; col < img.width; ++col)
    {
      auto& pel = img.pels[col + row * img.stride];

      if(pel.a > 0.5)
        pel.a = 1.0;
    }
  }
}

void blur(Image img)
{
  static auto const blurSize = 2;

  for(int row = 0; row < img.height; ++row)
  {
    for(int col = 0; col < img.width; ++col)
    {
      auto& pel = img.pels[col + row * img.stride];

      if(pel.a != 1)
        continue;

      auto sum = Vec3 {};
      int count = 0;

      for(int dy = -blurSize; dy <= blurSize; ++dy)
      {
        for(int dx = -blurSize; dx <= blurSize; ++dx)
        {
          auto x = clamp(col + dx, 0, img.width - 1);
          auto y = clamp(row + dy, 0, img.height - 1);
          auto nb = img.pels[x + y * img.stride];

          if(nb.a == 1.0)
          {
            sum = sum + Vec3 { nb.r, nb.g, nb.b };
            ++count;
          }
        }
      }

      sum = sum * (1.0 / count);
      pel.r = sum.x;
      pel.g = sum.y;
      pel.b = sum.z;
    }
  }
}


