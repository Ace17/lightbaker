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
Vec3 lightPos[2];
Vec3 lightColor[2];

Pixel fragmentShader(Vec3 pos, Vec3 N)
{
  Vec3 r {};

  for(int i = 0; i < 2; ++i)
  {
    auto lightVector = lightPos[i] - pos;
    auto dist = sqrt(dotProduct(lightVector, lightVector));
    auto cosTheta = dotProduct(lightVector * (1.0 / dist), N);
    float lightness = max(0, cosTheta) * 10.0f / (dist * dist);
    r = r + Vec3 { lightness* lightColor[i].x,
                            lightness* lightColor[i].y,
                            lightness* lightColor[i].z };
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
        auto N = a1.N * bary.x + a2.N * bary.y + a3.N * bary.z;
        colorBuffer[x] = fragmentShader(pos, N);
      }
    }

    colorBuffer += img.stride;
  }
}

void bakeLightmap(Scene& s, Image img)
{
  lightPos[0] = s.lights[0].pos;
  lightColor[0] = s.lights[0].color;
  lightPos[1] = s.lights[1].pos;
  lightColor[1] = s.lights[1].color;

  for(auto& t : s.triangles)
  {
    Attributes attr[3];

    for(int i = 0; i < 3; ++i)
    {
      attr[i].pos = t.v[i].pos;
      attr[i].N = t.v[i].N;
    }

    rasterizeTriangle(img,
                      t.v[0].uvLightmap, attr[0],
                      t.v[1].uvLightmap, attr[1],
                      t.v[2].uvLightmap, attr[2]);
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
#include <cstring> // memcmp

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
        vertices[i1 - 1],
        vertices[i2 - 1],
        vertices[i3 - 1]
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

template<typename T>
struct Span
{
  T* ptr;
  size_t len;

  T & operator [] (int i) { return ptr[i]; }
  void operator ++ () { ptr++; len--; }
};

using String = Span<char>;

Scene loadSceneAsObj(const char* path)
{
  Scene s;

  FILE* fp = fopen(path, "rb");
  assert(fp);

  static auto parseWord = [] (String& line) -> String
    {
      auto r = line;

      while(line.len > 0 && line[0] != '\n' && line[0] != ' ')
        ++line;

      r.len = line.ptr - r.ptr;

      if(line.len > 0)
        ++line;

      return r;
    };

  static auto parseFloat = [] (String& line)
    {
      return atof(parseWord(line).ptr);
    };

  static auto compare = [] (String s, const char* word)
    {
      auto n = strlen(word);

      if(s.len != n)
        return false;

      return memcmp(s.ptr, word, n) == 0;
    };

  std::vector<Vec3> v;
  std::vector<Vec3> vn;
  std::vector<Vec2> vt;

  char buffer[256] {};

  while(fgets(buffer, (sizeof buffer) - 1, fp))
  {
    auto line = String { buffer, strlen(buffer) };

    if(line[0] == '#')
      continue;

    auto word = parseWord(line);

    if(compare(word, "v"))
    {
      Vec3 a;
      a.x = parseFloat(line);
      a.y = parseFloat(line);
      a.z = parseFloat(line);
      v.push_back(a);
    }
    else if(compare(word, "vt"))
    {
      Vec2 a;
      a.x = parseFloat(line);
      a.y = parseFloat(line);
      vt.push_back(a);
    }
    else if(compare(word, "vn"))
    {
      Vec3 a;
      a.x = parseFloat(line);
      a.y = parseFloat(line);
      a.z = parseFloat(line);
      vn.push_back(a);
    }
    else if(compare(word, "f"))
    {
      std::vector<Vertex> vertices;

      while(1)
      {
        auto w = parseWord(line);

        if(w.len == 0)
          break;

        int iv, ivt, ivn;
        int n = sscanf(w.ptr, "%d/%d/%d", &iv, &ivt, &ivn);
        assert(n == 3);

        vertices.push_back({ v[iv - 1], vn[ivn - 1], vt[ivt - 1] });

        if(vertices.size() > 2)
        {
          Triangle t;
          t.v[0] = vertices[0];
          t.v[1] = vertices[vertices.size() - 2];
          t.v[2] = vertices[vertices.size() - 1];
          s.triangles.push_back(t);
        }
      }
    }
  }

  fclose(fp);

  return s;
}

void dumpScene(Scene const& s, const char* filename)
{
  FILE* fp = fopen(filename, "wb");
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

  fclose(fp);
}

void dumpSceneAsObj(Scene const& s, const char* filename)
{
  FILE* fp = fopen(filename, "wb");
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

  fprintf(fp, "# %d vertices\n", (int)allVertices.size());

#define FMT "%f"

  for(auto& vertex : allVertices)
  {
    fprintf(fp, "v " FMT " " FMT " " FMT "\n",
            vertex.pos.x, vertex.pos.y, vertex.pos.z);
  }

  for(auto& vertex : allVertices)
  {
    fprintf(fp, "vn " FMT " " FMT " " FMT "\n",
            vertex.N.x, vertex.N.y, vertex.N.z);
  }

  for(auto& vertex : allVertices)
  {
    fprintf(fp, "vt " FMT " " FMT "\n",
            vertex.uvLightmap.x, vertex.uvLightmap.y);
  }

#undef FMT

  int k = 0;

  for(auto i : allIndices)
  {
    if(k % 3 == 0)
      fprintf(fp, "f");

    int idx = i + 1;
    fprintf(fp, " %d/%d/%d", idx, idx, idx);

    if((k + 1) % 3 == 0)
      fprintf(fp, "\n");

    ++k;
  }

  fclose(fp);
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
  s = loadSceneAsObj("scene.obj");

  // manually add a light
  s.lights.push_back({
    { 2, 1, 3 }, { 0.0, 0.4, 0.5 }, 0.01
  });
  s.lights.push_back({
    { 0, 0, 0.12 }, { 0.01, 0.01, 0.0 }, 0.01
  });

  packTriangles(s);
  dumpScene(s, "mesh.out.txt");
  dumpSceneAsObj(s, "mesh.out.obj");

  Image img;
  img.stride = img.width = img.height = 2048;
  std::vector<Pixel> pixelData(img.width* img.height);
  img.pels = pixelData.data();

  bakeLightmap(s, img);
  writeTarga(img, "lightmap.tga");

  return 0;
}

