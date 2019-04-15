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

  static auto convert = [] (float value)
    {
      return (uint8_t)clamp(int(value * 256.0), 0, 255);
    };

  for(int row = 0; row < img.height; ++row)
  {
    for(int col = 0; col < img.width; ++col)
    {
      int srcOffset = col + row * img.stride;
      int dstOffset = col + row * img.width;

      pixelData[dstOffset * 4 + 0] = convert(img.pels[srcOffset].b);
      pixelData[dstOffset * 4 + 1] = convert(img.pels[srcOffset].g);
      pixelData[dstOffset * 4 + 2] = convert(img.pels[srcOffset].r);
      pixelData[dstOffset * 4 + 3] = convert(img.pels[srcOffset].a);
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

void computeNormals(Scene& s)
{
  for(auto& t : s.triangles)
    t.N = normalize(crossProduct(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos));
}

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

