#include "vec.h"
#include "scene.h"
#include "image.h"
#include "wavefront.h"

// packer.cpp
void packTriangles(Scene& s);

// lightmapp.cpp
Vec3 normalize(Vec3 vec);
void bakeLightmap(Scene& s, Image img);
void expandBorders(Image img);
void blur(Image img);

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

void computeNormals(Scene& s)
{
  for(auto& t : s.triangles)
    t.N = normalize(crossProduct(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos));
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

int main(int argc, char* argv[])
{
  if(argc != 2)
  {
    fprintf(stderr, "Usage: %s <scene.obj>\n", argv[0]);
    return 1;
  }

  auto s = loadSceneAsObj(argv[1]);

  computeNormals(s);

  // manually add lights
  s.lights.push_back({
    { 2, 1, 3 }, { 0.0, 0.4, 0.5 }, 0.01
  });
  s.lights.push_back({
    { 0, 0, 5 }, { 0.2, 0.2, 0.0 }, 0.01
  });

  packTriangles(s);
  dumpSceneAsObj(s, "out/mesh.obj");

  Image img;
  img.stride = img.width = img.height = 2048;
  std::vector<Pixel> pixelData(img.width* img.height);
  img.pels = pixelData.data();

  bakeLightmap(s, img);

  for(int i = 0; i < 8; ++i)
    expandBorders(img);

  blur(img);

  writeTarga(img, "out/lightmap.tga");

  return 0;
}

