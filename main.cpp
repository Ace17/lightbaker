// -----------------------------------------------------------------------------
// vec.h
struct Vec3
{
  float x, y, z;

  Vec3 operator * (float f) const { return { x* f, y* f, z* f }; }
  Vec3 operator + (Vec3 other) const { return { x + other.x, y + other.y, z + other.z }; }
  Vec3 operator - (Vec3 other) const { return { x - other.x, y - other.y, z - other.z }; }
};

float dotProduct(Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct Vec2
{
  float x, y;

  Vec2 operator - (Vec2 other) const { return { x - other.x, y - other.y }; }
};

float dotProduct(Vec2 a, Vec2 b)
{
  return a.x * b.x + a.y * b.y;
}

// -----------------------------------------------------------------------------
// scene.h

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
  Vertex v[3];
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

// -----------------------------------------------------------------------------
// image.h

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

inline float clamp(float val, float min, float max)
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

// -----------------------------------------------------------------------------
// packer.cpp
#include <cmath>

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

    auto topLeft = Vec2 { col* step, row* step };
    auto botLeft = Vec2 { col* step, row* step + size };
    auto topRight = Vec2 { col* step + size, row* step };

    t.v[0].uvLightmap = topLeft;
    t.v[1].uvLightmap = botLeft;
    t.v[2].uvLightmap = topRight;

    ++index;
  }
}

// -----------------------------------------------------------------------------
// lightmapp.cpp

// uniforms
Vec3 lightPos;
Vec3 lightColor;

Pixel fragmentShader(Vec3 pos)
{
  auto delta = pos - lightPos;
  float lightness = 10.0f / dotProduct(delta, delta);
  return { lightness* lightColor.x,
                                 lightness* lightColor.y,
                                 lightness* lightColor.z,
           1 };
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
};

void rasterizeTriangle(Image img, Vec2 v1, Attributes a1, Vec2 v2, Attributes a2, Vec2 v3, Attributes a3)
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
  auto const minx = (int)clamp(min(min(x1, x2), x3), 0, img.width);
  auto const maxx = (int)clamp(max(max(x1, x2), x3), 0, img.width);
  auto const miny = (int)clamp(min(min(y1, y2), y3), 0, img.height);
  auto const maxy = (int)clamp(max(max(y1, y2), y3), 0, img.height);

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
        colorBuffer[x] = fragmentShader(pos);
      }
    }

    colorBuffer += img.stride;
  }
}

void bakeLightmap(Scene& s, Image img)
{
  lightPos = s.lights[0].pos;
  lightColor = s.lights[0].color;

  for(auto& t : s.triangles)
  {
    Attributes a1;
    a1.pos = t.v[0].pos;
    Attributes a2;
    a2.pos = t.v[1].pos;
    Attributes a3;
    a3.pos = t.v[2].pos;
    rasterizeTriangle(img,
                      t.v[0].uvLightmap, a1,
                      t.v[1].uvLightmap, a2,
                      t.v[2].uvLightmap, a3);
  }
}

// -----------------------------------------------------------------------------
// write_tga.cpp
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>

void writeTarga(Image img, const char* filename)
{
  uint8_t hdr[18] =
  {
    0, 0,
    (uint8_t)(2),
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    (uint8_t)((img.width >> 0) & 0xff),
    (uint8_t)((img.width >> 8) & 0xff),
    (uint8_t)((img.height >> 0) & 0xff),
    (uint8_t)((img.height >> 8) & 0xff),
    (uint8_t)(32),
    (uint8_t)(8)
  };

  std::vector<uint8_t> pixelData(img.width* img.height * 4);

  for(int row = 0; row < img.height; ++row)
  {
    for(int col = 0; col < img.width; ++col)
    {
      int srcOffset = col + row * img.stride;
      int dstOffset = col + row * img.width;

      pixelData[dstOffset * 4 + 0] = (uint8_t)clamp(img.pels[srcOffset].b * 256.0, 0, 255);
      pixelData[dstOffset * 4 + 1] = (uint8_t)clamp(img.pels[srcOffset].g * 256.0, 0, 255);
      pixelData[dstOffset * 4 + 2] = (uint8_t)clamp(img.pels[srcOffset].r * 256.0, 0, 255);
      pixelData[dstOffset * 4 + 3] = (uint8_t)clamp(img.pels[srcOffset].a * 256.0, 0, 255);
    }
  }

  FILE* file = fopen(filename, "wb");
  assert(file);

  fwrite(hdr, 1, sizeof(hdr), file);
  fwrite(pixelData.data(), 1, pixelData.size(), file);

  fclose(file);
}

// -----------------------------------------------------------------------------
// main.cpp
#include <cstdio>

Scene createDummyScene()
{
  Scene s;
  Vertex v1, v2, v3;
  v1.pos = { 0, 0, 0 };
  v2.pos = { 4, 0, 0 };
  v3.pos = { 0, 4, 0 };
  v1.uvDiffuse = { 0, 0 };
  v2.uvDiffuse = { 1, 0 };
  v3.uvDiffuse = { 0, 1 };
  v1.N = v2.N = v3.N = { 0, 0, 1 };
  s.triangles.push_back({ v1, v2, v3 });

  v1.pos.x += 5;
  v2.pos.x += 5;
  v3.pos.x += 5;
  s.triangles.push_back({ v1, v2, v3 });

  return s;
}

Scene loadScene(const char* path)
{
  std::vector<Vertex> vertices;

  Scene s;

  FILE* fp = fopen(path, "rb");
  assert(fp);
  char buffer[256];
  bool triangleMode = false;

  while(fgets(buffer, sizeof buffer, fp))
  {
    if(buffer[0] == '#')
      continue;

    if(buffer[0] == '@')
    {
      triangleMode = true;
      continue;
    }

    if(triangleMode)
    {
      Triangle triangle;
      int i1, i2, i3;
      int count = sscanf(buffer, "%d %d %d", &i1, &i2, &i3);
      assert(count == 3);
      s.triangles.push_back({
        vertices[i1],
        vertices[i2],
        vertices[i3]
      });
    }
    else
    {
      Vertex vertex;
      int count = sscanf(buffer,
                         "%f %f %f - %f %f %f",
                         &vertex.pos.x, &vertex.pos.y, &vertex.pos.z,
                         &vertex.N.x, &vertex.N.y, &vertex.N.z);
      assert(count == 6);
      vertices.push_back(vertex);
    }
  }

  fclose(fp);

  return s;
}

void dumpScene(Scene const& s, FILE* fp)
{
  std::vector<Vertex> allVertices;
  std::vector<int> allIndices;

  for(auto& t : s.triangles)
  {
    for(auto& vertex : t.v)
    {
      auto i = (int)allVertices.size();
      allVertices.push_back(vertex);
      allIndices.push_back(i);
    }
  }

  fprintf(fp, "# generated\n");

  fprintf(fp, "vertices\n");

  for(auto& vertex : allVertices)
  {
#define FMT "%.1f"
    fprintf(fp, FMT " " FMT " " FMT " - "
            FMT " " FMT " " FMT " - "
            FMT " " FMT " - "
            FMT " " FMT "\n",
            vertex.pos.x,
            vertex.pos.y,
            vertex.pos.z,
            vertex.N.x,
            vertex.N.y,
            vertex.N.z,
            vertex.uvDiffuse.x,
            vertex.uvDiffuse.y,
            vertex.uvLightmap.x,
            vertex.uvLightmap.y
            );
#undef FMT
  }

  fprintf(fp, "triangles\n");
  int k = 0;

  for(auto i : allIndices)
  {
    if(k % 3)
      fprintf(fp, " ");

    fprintf(fp, "%d", i);

    if((k + 1) % 3 == 0)
      fprintf(fp, "\n");

    ++k;
  }
}

using namespace std;

// # input file format example
// vertices
// 0 0 0 - 0 0 0 - 0 0
// triangles
// 0 0 0
// 0 0 0
// 0 0 0
// omnilights
// 0 0 0 - 0 0 0

int main()
{
  auto s = createDummyScene();
  s = loadScene("mesh.txt");

  // manually add a light
  s.lights.push_back({
    { 2, 1, 3 }, { 0.0, 0.4, 0.5 }, 0.01
  });

  packTriangles(s);
  dumpScene(s, stdout);

  Image img;
  img.stride = img.width = img.height = 2048;
  std::vector<Pixel> pixelData(img.width* img.height);
  img.pels = pixelData.data();

  bakeLightmap(s, img);
  writeTarga(img, "lightmap.tga");

  return 0;
}

