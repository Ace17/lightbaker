// read/write Wavefront (.obj) 3D mesh files.
#include "wavefront.h"

#include "scene.h"
#include "span.h"
#include <cstdio>
#include <cmath> // atof
#include <cstring> // strlen, memcmp
#include <cassert>

using String = Span<char>;

Scene loadSceneAsObj(const char* filename)
{
  Scene s;

  FILE* fp = fopen(filename, "rb");
  assert(fp);

  static auto parseWord = [] (String& line) -> String
    {
      auto r = line;

      while(line.len > 0 && line[0] != '\n' && line[0] != ' ')
        ++line;

      r.len = line.data - r.data;

      if(line.len > 0)
        ++line;

      return r;
    };

  static auto parseFloat = [] (String& line)
    {
      return atof(parseWord(line).data);
    };

  static auto compare = [] (String s, const char* word)
    {
      auto n = strlen(word);

      if(s.len != n)
        return false;

      return memcmp(s.data, word, n) == 0;
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
        int n = sscanf(w.data, "%d/%d/%d", &iv, &ivt, &ivn);
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

void dumpSceneAsObj(Scene const& s, const char* filename)
{
  FILE* fp = fopen(filename, "wb");
  assert(fp);

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

  fprintf(fp, "mtllib mesh.mtl\n");
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

