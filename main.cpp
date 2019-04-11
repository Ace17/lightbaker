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

Vec3 crossProduct(Vec3 a, Vec3 b)
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

// -----------------------------------------------------------------------------
// lightmapp.cpp

Vec3 normalize(Vec3 vec)
{
  auto const magnitude = dotProduct(vec, vec);
  return vec * (1.0 / sqrt(magnitude));
}

// return 'false' if the ray hit something
bool raycast(Triangle const& t, Vec3 rayStart, Vec3 rayDelta)
{
  // TODO: precompute this
  auto N = normalize(crossProduct(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos));

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

      pixelData[dstOffset * 4 + 0] = (uint8_t)clamp(int(img.pels[srcOffset].b * 256.0), 0, 255);
      pixelData[dstOffset * 4 + 1] = (uint8_t)clamp(int(img.pels[srcOffset].g * 256.0), 0, 255);
      pixelData[dstOffset * 4 + 2] = (uint8_t)clamp(int(img.pels[srcOffset].r * 256.0), 0, 255);
      pixelData[dstOffset * 4 + 3] = (uint8_t)clamp(int(img.pels[srcOffset].a * 256.0), 0, 255);
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

  fprintf(fp, "mtllib mesh.out.mtl\n");
  fprintf(fp, "o FullMesh\n");
  fprintf(fp, "usemtl Material.001\n");

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

  // manually add lights
  s.lights.push_back({
    { 2, 1, 3 }, { 0.0, 0.4, 0.5 }, 0.01
  });
  s.lights.push_back({
    { 0, 0, 5 }, { 0.2, 0.2, 0.0 }, 0.01
  });

  packTriangles(s);
  dumpScene(s, "mesh.out.txt");
  dumpSceneAsObj(s, "mesh.out.obj");

  Image img;
  img.stride = img.width = img.height = 4096;
  std::vector<Pixel> pixelData(img.width* img.height);
  img.pels = pixelData.data();

  bakeLightmap(s, img);

  for(int i = 0; i < 8; ++i)
    expandBorders(img);

  blur(img);

  writeTarga(img, "lightmap.tga");

  return 0;
}

