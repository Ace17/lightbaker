#include <cstdlib>

// returns false if triangles do not fit into the rect with the specified size, border and spacing
bool tpPackIntoRect(const float* positions, int vertexCount, int width, int height, int border, int spacing, float* outUVs, float* outScale3Dto2D);

// returns number of successfully packed vertices
int tpPackWithFixedScaleIntoRect(const float* positions, int vertexCount, float scale3Dto2D, int width, int height, int border, int spacing, float* outUVs);

////////////////////// END OF HEADER //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

static inline int tp_mini(int a, int b) { return a < b ? a : b; }
static inline int tp_maxi(int a, int b) { return a > b ? a : b; }
static inline int tp_absi(int a) { return a < 0 ? -a : a; }

struct Vec2
{
  float x, y;
};

static inline Vec2 tp_v2i(int x, int y)
{
  Vec2 v = { (float)x, (float)y };
  return v;
}

static inline Vec2 tp_mul2(Vec2 a, Vec2 b) { return { a.x* b.x, a.y* b.y }; }

struct Vec3
{
  float x, y, z;
};

static inline Vec3 tp_add3(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline Vec3 tp_sub3(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline Vec3 tp_scale3(Vec3 a, float b) { return { a.x* b, a.y* b, a.z* b }; }
static inline Vec3 tp_div3(Vec3 a, float b) { return tp_scale3(a, 1.0f / b); }
static inline float tp_dot3(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float tp_length3sq(Vec3 a) { return a.x * a.x + a.y * a.y + a.z * a.z; }
static inline float tp_length3(Vec3 a) { return sqrtf(tp_length3sq(a)); }
static inline Vec3 tp_normalize3(Vec3 a) { return tp_div3(a, tp_length3(a)); }

struct tp_triangle
{
  int Aindex;
  short w, x, h, hflip;
  // |        C           -
  // |      * |  *        | h
  // |    *   |     *     |
  // |  B-----+--------A  -
  // |  '--x--'        |
  // |  '-------w------'
};

static int tp_triangle_cmp(const void* a, const void* b)
{
  auto ea = (tp_triangle*)a;
  auto eb = (tp_triangle*)b;
  int dh = eb->h - ea->h;
  return dh != 0 ? dh : (eb->w - ea->w);
}

static void tp_wave_surge(int* wave, int right, int x0, int y0, int x1, int y1)
{
  int dx = tp_absi(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = tp_absi(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;

  for(;;)
  {
    if(right)
      wave[y0] = x0 > wave[y0] ? x0 : wave[y0];
    else
      wave[y0] = x0 < wave[y0] ? x0 : wave[y0];

    if(x0 == x1 && y0 == y1)
      break;

    e2 = err;

    if(e2 > -dx)
    {
      err -= dy;
      x0 += sx;
    }

    if(e2 < dy)
    {
      err += dx;
      y0 += sy;
    }
  }
}

static int tp_wave_wash_up(int* wave, int right, int height, int y0, int x1, int y1, int spacing)
{
  int x0 = 0;
  int dx = tp_absi(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = tp_absi(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;
  int x = wave[y0];

  for(;;)
  {
    int xDistance = wave[y0] - x0 - x;

    for(int y = tp_maxi(y0 - spacing, 0); y <= tp_mini(y0 + spacing, height - 1); y++)
      xDistance = right ? tp_maxi(wave[y] - x0 - x, xDistance) : tp_mini(wave[y] - x0 - x, xDistance);

    if((right && xDistance > 0) || (!right && xDistance < 0))
      x += xDistance;

    if(x0 == x1 && y0 == y1)
      break;

    e2 = err;

    if(e2 > -dx)
    {
      err -= dy;
      x0 += sx;
    }

    if(e2 < dy)
    {
      err += dx;
      y0 += sy;
    }
  }

  return x;
}

int tpPackWithFixedScaleIntoRect(const float* positions, int vertexCount, float scale3Dto2D, const int width, const int height, int border, int spacing, float* outUVs)
{
  tp_triangle* tris = new tp_triangle[vertexCount / 3];
  auto p = (Vec3*)positions;
  Vec2* uv = (Vec2*)outUVs;

  for(int i = 0; i < vertexCount / 3; i++)
  {
    Vec3 tp[3], tv[3];
    tp[0] = tp_scale3(p[i * 3 + 0], scale3Dto2D);
    tp[1] = tp_scale3(p[i * 3 + 1], scale3Dto2D);
    tp[2] = tp_scale3(p[i * 3 + 2], scale3Dto2D);
    tv[0] = tp_sub3(tp[1], tp[0]);
    tv[1] = tp_sub3(tp[2], tp[1]);
    tv[2] = tp_sub3(tp[0], tp[2]);
    float tvlsq[3] = { tp_length3sq(tv[0]), tp_length3sq(tv[1]), tp_length3sq(tv[2]) };

    // find long edge
    int maxi;
    float maxl = tvlsq[0];
    maxi = 0;

    if(tvlsq[1] > maxl)
    {
      maxl = tvlsq[1];
      maxi = 1;
    }

    if(tvlsq[2] > maxl)
    {
      maxl = tvlsq[2];
      maxi = 2;
    }

    int nexti = (maxi + 1) % 3;

    // measure triangle
    float w = sqrtf(maxl);
    float x = -tp_dot3(tv[maxi], tv[nexti]) / w;
    float h = tp_length3(tp_sub3(tp_add3(tv[maxi], tv[nexti]), tp_scale3(tp_normalize3(tv[maxi]), w - x)));

    // store entry
    tp_triangle* e = tris + i;
    e->Aindex = i * 3 + maxi;
    e->w = (int)ceilf(w);
    e->x = (int)ceilf(x);
    e->h = (int)ceilf(h);
    e->hflip = 0;
  }

  qsort(tris, vertexCount / 3, sizeof(tp_triangle), tp_triangle_cmp);

  int processed;
  int* waves[2];
  waves[0] = (int*)calloc(2 * height, sizeof(int));
  waves[1] = waves[0] + height;

  for(int i = 0; i < height; i++)
  {
    waves[0][i] = width - 1;// - border;
    waves[1][i] = border;
  }

  int pass = 0;

  int row_y = border;
  int row_h = tris[0].h;
  int vflip = 0;

  for(processed = 0; processed < vertexCount / 3; processed++)
  {
    tp_triangle* e = tris + processed;
    int ymin, ystart, yend, xmin[2], x, hflip;
    retry:
    ymin = vflip ? row_y + row_h - e->h : row_y;
    ystart = vflip ? ymin + e->h : ymin;
    yend = vflip ? ymin : ymin + e->h;

    // calculate possible x values for the triangle in the current row
    hflip = processed & 1; // seems to work better than the heuristics below!?

    if(pass < 3) // left to right (first three passes)
    {
      xmin[0] = tp_wave_wash_up(waves[1], 1, height, ystart, e->x, yend, spacing);
      xmin[1] = tp_wave_wash_up(waves[1], 1, height, ystart, e->w - e->x, yend, spacing); // flipped horizontally
      // hflip = (xmin[1] < xmin[0] || (xmin[1] == xmin[0] && e->x > e->w / 2)) ? 1 : 0;
    }
    else if(pass < 5) // right to left (last two passes)
    { // TODO: fix spacing!
      xmin[0] = tp_wave_wash_up(waves[0], 0, height, ystart, -e->x, yend, spacing) - e->w - 1;
      xmin[1] = tp_wave_wash_up(waves[0], 0, height, ystart, -e->w + e->x, yend, spacing) - e->w - 1; // flipped horizontally
      // hflip = (xmin[1] > xmin[0] || (xmin[1] == xmin[0] && e->x < e->w / 2)) ? 1 : 0;
    }
    else
      goto finish;

    // execute hflip (and choose best x)
    e->x = hflip ? e->w - e->x : e->x;
    e->hflip ^= hflip;
    x = xmin[hflip];

    // check if it fits into the specified rect
    // (else advance to next row or do another pass over the rect)
    if(x + e->w + border >= width || x < border)
    {
      row_y += row_h + spacing + 1; // next row
      row_h = e->h;

      if(row_y + row_h + border >= height)
      {
        ++pass; // next pass
        row_y = border;
      }

      goto retry;
    }

    // found a space for the triangle. update waves
    tp_wave_surge(waves[0], 0, x - spacing - 1, ystart, x + e->x - spacing - 1, yend); // left side
    tp_wave_surge(waves[1], 1, x + e->w + spacing + 1, ystart, x + e->x + spacing + 1, yend); // right side

    // calc & store UVs
    if(uv)
    {
      int tri = e->Aindex - (e->Aindex % 3);
      int Ai = e->Aindex;
      int Bi = tri + ((e->Aindex + 1) % 3);
      int Ci = tri + ((e->Aindex + 2) % 3);

      if(e->hflip)
      {
        auto tmp = Ai;
        Ai = Bi;
        Bi = tmp;
      }

      auto const uvScale = Vec2 { 1.0f / width, 1.0f / height };

      uv[Ai] = tp_mul2(tp_v2i(x + e->w, ystart), uvScale);
      uv[Bi] = tp_mul2(tp_v2i(x, ystart), uvScale);
      uv[Ci] = tp_mul2(tp_v2i(x + e->x, yend), uvScale);
    }

    vflip = !vflip;
  }

  finish:

  free(waves[0]);
  delete[] tris;

  return processed * 3;
}

bool tpPackIntoRect(const float* positions, int vertexCount, int width, int height, int border, int spacing, float* outUVs, float* outScale3Dto2D)
{
  float testScale = 1.0f;
  float lastFitScale = 0.0f;
  float multiplicator = 0.5f;

  int processed = tpPackWithFixedScaleIntoRect(positions, vertexCount, testScale, width, height, border, spacing, 0);
  bool increase = processed >= vertexCount;

  if(increase)
  {
    while(!(processed < vertexCount))
    {
      testScale *= 2.0f;
      // printf("inc testing scale %f\n", testScale);
      processed = tpPackWithFixedScaleIntoRect(positions, vertexCount, testScale, width, height, border, spacing, 0);
    }

    lastFitScale = testScale / 2.0f;
    // printf("inc scale %f fits\n", lastFitScale);
    multiplicator = 0.75f;
  }

  for(int j = 0; j < 16; j++)
  {
    // printf("dec multiplicator %f\n", multiplicator);
    for(int i = 0; processed < vertexCount && i < 2; i++)
    {
      testScale *= multiplicator;
      // printf("dec testing scale %f\n", testScale);
      processed = tpPackWithFixedScaleIntoRect(positions, vertexCount, testScale, width, height, border, spacing, 0);
    }

    if(!(processed < vertexCount))
    {
      processed = 0;
      // printf("scale %f fits\n", testScale);
      lastFitScale = testScale;
      testScale /= multiplicator;
      multiplicator = (multiplicator + 1.0f) * 0.5f;
    }
  }

  if(lastFitScale > 0.0f)
  {
    *outScale3Dto2D = lastFitScale;
    processed = tpPackWithFixedScaleIntoRect(positions, vertexCount, lastFitScale, width, height, border, spacing, outUVs);
    assert(processed == vertexCount);
    return true;
  }

  return false;
}

int main()
{
  return 0;
}

