// Tests for the STL exporter's geometry half, compiled straight from the upstream plugin
// source (upstream/Source/plugins/exporters/stl/PrintMesh.cpp). Pure C++ and glm: no Qt, no
// game data, no window -- the same reason the exporter keeps that half free of WoWModel.
//
// What is proven here is what a unit or winding mistake would silently corrupt: the height in
// the file, the placement on the plate, the outward normals under a mirrored transform, the
// footprint measurement, the open-sheet detection and the binary STL format itself.
//
//   stltest
//
// Exit code = number of failing checks, so a plain shell/CI check works.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "glm/gtc/matrix_transform.hpp"

#include "PrintMesh.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
      failures++;                                                                \
    }                                                                            \
  } while (0)

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// A closed tetrahedron with outward winding, corners at the origin and the three unit axes.
// Every normal below was checked by hand from the cross products.
const glm::vec3 A(0, 0, 0), B(1, 0, 0), C(0, 1, 0), D(0, 0, 1);

PrintMesh::Triangles tetra(const glm::mat4& m = glm::mat4(1.0f))
{
  PrintMesh::Triangles t;
  PrintMesh::appendTriangle(t, m, A, C, B);   // bottom, normal -z
  PrintMesh::appendTriangle(t, m, A, B, D);   // normal -y
  PrintMesh::appendTriangle(t, m, A, D, C);   // normal -x
  PrintMesh::appendTriangle(t, m, B, C, D);   // normal +(1,1,1)
  return t;
}

// Six times the signed volume enclosed by outward-wound triangles. Positive means every
// face points outward -- the one global test of winding that does not need to know which
// face is which.
float signedVolume6(const PrintMesh::Triangles& tris)
{
  float v = 0.0f;
  for (const PrintMesh::Triangle& t : tris)
    v += glm::dot(t.v[0], glm::cross(t.v[1], t.v[2]));
  return v;
}

void testBoundsAndFit()
{
  PrintMesh::Triangles t = tetra();
  PrintMesh::Bounds b = PrintMesh::bounds(t);
  CHECK(b.valid);
  CHECK(near(b.min.x, 0) && near(b.min.y, 0) && near(b.min.z, 0));
  CHECK(near(b.max.x, 1) && near(b.max.y, 1) && near(b.max.z, 1));

  const float s = PrintMesh::fitToHeight(t, 200.0f);
  CHECK(near(s, 200.0f));
  b = PrintMesh::bounds(t);
  // Height exact, standing on the plate, centred in X and Y.
  CHECK(near(b.max.z - b.min.z, 200.0f));
  CHECK(near(b.min.z, 0.0f));
  CHECK(near(b.min.x, -100.0f) && near(b.max.x, 100.0f));
  CHECK(near(b.min.y, -100.0f) && near(b.max.y, 100.0f));
  // Fitting is idempotent.
  CHECK(near(PrintMesh::fitToHeight(t, 200.0f), 1.0f));

  // A part that lies: a sword-shaped box, long along X. The requested size goes on the long
  // side, and the piece still ends up on the plate and centred.
  PrintMesh::Triangles sword;
  const glm::mat4 stretch = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, 1.0f, 0.5f));
  for (const PrintMesh::Triangle& tri : tetra(stretch))
    sword.push_back(tri);
  CHECK(PrintMesh::longestAxis(PrintMesh::bounds(sword)) == 0);
  CHECK(PrintMesh::longestAxis(PrintMesh::bounds(tetra())) == 2);   // a tie stands
  CHECK(near(PrintMesh::fitExtent(sword, 0, 100.0f), 10.0f));
  b = PrintMesh::bounds(sword);
  CHECK(near(b.max.x - b.min.x, 100.0f));
  CHECK(near(b.max.z - b.min.z, 5.0f));
  CHECK(near(b.min.z, 0.0f));
  CHECK(near(b.min.x, -50.0f) && near(b.max.x, 50.0f));
  CHECK(PrintMesh::fitExtent(sword, 3, 100.0f) == 0.0f);            // no such axis

  // Nothing to scale: an empty list and a flat one.
  PrintMesh::Triangles empty;
  CHECK(PrintMesh::fitToHeight(empty, 200.0f) == 0.0f);
  CHECK(!PrintMesh::bounds(empty).valid);
  PrintMesh::Triangles flat;
  PrintMesh::appendTriangle(flat, glm::mat4(1.0f), A, B, C);
  CHECK(PrintMesh::fitToHeight(flat, 200.0f) == 0.0f);
}

void testWinding()
{
  // Untouched: outward.
  CHECK(signedVolume6(tetra()) > 0.0f);

  // A rotation keeps the winding.
  const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), 1.2f, glm::vec3(0.3f, 0.5f, 0.8f));
  CHECK(signedVolume6(tetra(rot)) > 0.0f);

  // A mirror (the viewport's mirrored_ flag scales Y by -1) would turn every face inside
  // out; appendTriangle swaps the order back so the volume stays positive.
  const glm::mat4 mirror = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
  CHECK(signedVolume6(tetra(mirror)) > 0.0f);

  // ... and a mirror that also rotates and scales.
  const glm::mat4 both = rot * mirror * glm::scale(glm::mat4(1.0f), glm::vec3(3.0f));
  CHECK(signedVolume6(tetra(both)) > 0.0f);

  // The bug this guards against, stated directly: without the swap the mirrored volume is
  // negative.
  PrintMesh::Triangles naive;
  for (const PrintMesh::Triangle& t : tetra()) {
    PrintMesh::Triangle m;
    for (int i = 0; i < 3; i++)
      m.v[i] = glm::vec3(mirror * glm::vec4(t.v[i], 1.0f));
    naive.push_back(m);
  }
  CHECK(signedVolume6(naive) < 0.0f);
}

void testDegenerate()
{
  PrintMesh::Triangles t = tetra();
  PrintMesh::appendTriangle(t, glm::mat4(1.0f), A, A, B);              // zero area
  PrintMesh::appendTriangle(t, glm::mat4(1.0f), A, B, glm::vec3(2, 0, 0)); // collinear
  CHECK(t.size() == 6);
  CHECK(PrintMesh::dropDegenerate(t, PrintMesh::MIN_TRIANGLE_AREA_MM2) == 2);
  CHECK(t.size() == 4);
  CHECK(PrintMesh::dropDegenerate(t, PrintMesh::MIN_TRIANGLE_AREA_MM2) == 0);
}

void testContact()
{
  PrintMesh::Triangles t = tetra();
  PrintMesh::fitToHeight(t, 200.0f);
  // Three corners on the plate span the whole 200 x 200 base; the apex is 200 mm up and
  // must not count.
  glm::vec2 c = PrintMesh::contactFootprint(t, PrintMesh::CONTACT_SLAB_MM);
  CHECK(near(c.x, 200.0f) && near(c.y, 200.0f));

  // A figure on one toe: a tetrahedron standing on its apex.
  const glm::mat4 flip = glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(1, 0, 0));
  PrintMesh::Triangles toe = tetra(flip);
  PrintMesh::fitToHeight(toe, 200.0f);
  c = PrintMesh::contactFootprint(toe, PrintMesh::CONTACT_SLAB_MM);
  CHECK(c.x < PrintMesh::CONTACT_MIN_MM && c.y < PrintMesh::CONTACT_MIN_MM);

  PrintMesh::Triangles empty;
  c = PrintMesh::contactFootprint(empty, PrintMesh::CONTACT_SLAB_MM);
  CHECK(c.x == 0.0f && c.y == 0.0f);
}

void testOpenEdges()
{
  // The closed tetrahedron: every edge has two faces.
  const uint32_t closed[] = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
  CHECK(near(PrintMesh::openEdgeRatio(closed, 12), 0.0f));

  // A lone quad: five distinct edges, four of them open.
  const uint32_t quad[] = {0, 1, 2, 0, 2, 3};
  CHECK(near(PrintMesh::openEdgeRatio(quad, 6), 0.8f));
  CHECK(PrintMesh::openEdgeRatio(quad, 6) > PrintMesh::OPEN_SHEET_RATIO);

  // A single triangle is all edge.
  const uint32_t one[] = {0, 1, 2};
  CHECK(near(PrintMesh::openEdgeRatio(one, 3), 1.0f));

  // Collapsed edges are not edges; nothing to divide by.
  const uint32_t collapsed[] = {5, 5, 5};
  CHECK(PrintMesh::openEdgeRatio(collapsed, 3) == 0.0f);
  CHECK(PrintMesh::openEdgeRatio(nullptr, 0) == 0.0f);
}

void testBinarySTL()
{
  PrintMesh::Triangles t = tetra();
  PrintMesh::fitToHeight(t, 200.0f);

  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  CHECK(PrintMesh::writeBinarySTL(ss, t, "WMV test header"));
  const std::string bytes = ss.str();
  // 80-byte header, 4-byte count, 50 bytes per triangle.
  CHECK(bytes.size() == 84 + 50 * t.size());
  CHECK(bytes.compare(0, 15, "WMV test header") == 0);
  CHECK(bytes[15] == '\0');
  CHECK((unsigned char)bytes[80] == t.size() && bytes[81] == 0 && bytes[82] == 0 && bytes[83] == 0);

  // The first triangle's normal is the recomputed -z of the bottom face.
  float n[3];
  std::memcpy(n, bytes.data() + 84, sizeof(n));
  CHECK(near(n[0], 0.0f) && near(n[1], 0.0f) && near(n[2], -1.0f));

  ss.seekg(0);
  PrintMesh::STLInfo info;
  CHECK(PrintMesh::readBinarySTL(ss, info, PrintMesh::CONTACT_SLAB_MM));
  CHECK(info.triangles == t.size());
  CHECK(near(info.bounds.size().z, 200.0f));
  CHECK(near(info.bounds.min.z, 0.0f));
  CHECK(near(info.contact.x, 200.0f) && near(info.contact.y, 200.0f));

  // A header that begins with "solid" would make some readers try ASCII parsing.
  std::stringstream s2(std::ios::in | std::ios::out | std::ios::binary);
  CHECK(PrintMesh::writeBinarySTL(s2, t, "solid something"));
  CHECK(s2.str().compare(0, 5, "Solid") == 0);

  // A long header is cut, not overrun.
  std::stringstream s3(std::ios::in | std::ios::out | std::ios::binary);
  CHECK(PrintMesh::writeBinarySTL(s3, t, std::string(300, 'x').c_str()));
  CHECK(s3.str().size() == 84 + 50 * t.size());

  // Truncated input is refused, not misread.
  std::stringstream s4(bytes.substr(0, bytes.size() - 7), std::ios::in | std::ios::binary);
  CHECK(!PrintMesh::readBinarySTL(s4, info, PrintMesh::CONTACT_SLAB_MM));
  std::stringstream s5(bytes.substr(0, 40), std::ios::in | std::ios::binary);
  CHECK(!PrintMesh::readBinarySTL(s5, info, PrintMesh::CONTACT_SLAB_MM));

  // An empty mesh is a valid, empty file.
  PrintMesh::Triangles none;
  std::stringstream s6(std::ios::in | std::ios::out | std::ios::binary);
  CHECK(PrintMesh::writeBinarySTL(s6, none, "empty"));
  CHECK(s6.str().size() == 84);
  s6.seekg(0);
  CHECK(PrintMesh::readBinarySTL(s6, info, PrintMesh::CONTACT_SLAB_MM));
  CHECK(info.triangles == 0 && !info.bounds.valid);
}

} // namespace

int main()
{
  testBoundsAndFit();
  testWinding();
  testDegenerate();
  testContact();
  testOpenEdges();
  testBinarySTL();

  if (failures == 0)
    std::printf("stltest: all checks passed\n");
  else
    std::printf("stltest: %d check(s) FAILED\n", failures);
  return failures;
}
