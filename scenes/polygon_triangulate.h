#pragma once

// Triangulation of the faces the demo apps read from GeoNodus (whole polygons, maxVerticesPerPrimitive = -1). Shared
// by the apps of every OS: Windows calls it directly, the Swift apps through the C bridge (demo_scenes_c.h).
#include <cstdint>
#include <vector>

namespace demo {

// Splits a polygon of n vertices into triangles of indices base + 0 … base + n − 1 keeping the winding. positions — x,
// y, z of vertex 0, then of vertex 1 `stride` floats further, and so on.
//  - a triangle and a convex face — a fan;
//  - otherwise, in the projection onto the face plane: the face is split back into its outer contour and holes (Hole,
//    Boolean and Dissolve join a hole to the outer contour with a bridge — an edge walked there and back; bridges may
//    run along hole edges, which plain ear clipping does not survive), then triangulated by the earcut algorithm
//    (as mapbox earcut: its own bridges to holes, filtering of repeated and collinear points, curing of local
//    self-intersections and splitting when no ear is left).
void triangulatePolygon(const float* positions, int stride, int n, uint32_t base, std::vector<uint32_t>& tris);

} // namespace demo
