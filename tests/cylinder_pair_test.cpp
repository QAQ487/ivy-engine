// Cylinder pair contact tests.
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/cylinder_pair_test tests/cylinder_pair_test.cpp engine/physics/collider/cylinder_pair.cpp

#include "engine/physics/collider/cylinder_pair.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static float Len(CylinderPairVec3 v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static CylinderPairVec3 V(float x, float y, float z) {
    return { x, y, z };
}

static CylinderPairShape MakeBox(CylinderPairVec3 center, CylinderPairVec3 half) {
    CylinderPairShape s;
    s.kind = CylinderPairShapeKind::Box;
    s.center = center;
    s.axisX = V(1.0f, 0.0f, 0.0f);
    s.axisY = V(0.0f, 1.0f, 0.0f);
    s.axisZ = V(0.0f, 0.0f, 1.0f);
    s.extents = half;
    return s;
}

static CylinderPairShape MakeBoxYaw45(CylinderPairVec3 center, CylinderPairVec3 half) {
    CylinderPairShape s = MakeBox(center, half);
    const float c = 0.70710678118f;
    const float s45 = 0.70710678118f;
    s.axisX = V(c, 0.0f, -s45);
    s.axisZ = V(s45, 0.0f, c);
    return s;
}

static CylinderPairShape MakeCyl(CylinderPairVec3 center, CylinderPairVec3 axis, float radius, float halfHeight) {
    CylinderPairShape s;
    s.kind = CylinderPairShapeKind::Cylinder;
    s.center = center;
    s.axisY = axis;
    s.extents = V(radius, halfHeight, 0.0f);
    return s;
}

static CylinderPairShape MakeTri(CylinderPairVec3 a, CylinderPairVec3 b, CylinderPairVec3 c) {
    CylinderPairShape s;
    s.kind = CylinderPairShapeKind::Triangle;
    s.center = a;
    s.tri1 = b;
    s.tri2 = c;
    return s;
}

struct Hit {
    bool hit = false;
    CylinderPairVec3 n{};
    float pen = 0.0f;
    CylinderPairVec3 pt{};
};

static Hit Query(const CylinderPairShape& a, const CylinderPairShape& b) {
    Hit h;
    h.hit = CylinderPair_Contact(a, b, h.n, h.pen, &h.pt);
    return h;
}

static void ExpectHit(const Hit& h, CylinderPairVec3 n, float pen, const char* name) {
    if (!h.hit) {
        std::fprintf(stderr, "FAIL: %s expected a hit\n", name);
        ++g_failed;
        return;
    }
    const float nlen = Len(h.n);
    if (std::fabs(nlen - 1.0f) > 1e-3f) {
        std::fprintf(stderr, "FAIL: %s normal not unit (len %g)\n", name, nlen);
        ++g_failed;
    }
    const float nd = h.n.x * n.x + h.n.y * n.y + h.n.z * n.z;
    if (nd < 0.98f) {
        std::fprintf(stderr, "FAIL: %s normal (%g %g %g) expected about (%g %g %g)\n",
            name, h.n.x, h.n.y, h.n.z, n.x, n.y, n.z);
        ++g_failed;
    }
    if (std::fabs(h.pen - pen) > 2e-3f) {
        std::fprintf(stderr, "FAIL: %s penetration %g expected %g\n", name, h.pen, pen);
        ++g_failed;
    }
}

// Face and side contacts are often a line or a disc, so any witness in that
// patch is valid. The box is the slab the contact point must lie in.
static void ExpectPointBox(const Hit& h, CylinderPairVec3 mn, CylinderPairVec3 mx, const char* name) {
    if (!h.hit) return;
    if (h.pt.x < mn.x || h.pt.x > mx.x || h.pt.y < mn.y || h.pt.y > mx.y || h.pt.z < mn.z || h.pt.z > mx.z) {
        std::fprintf(stderr, "FAIL: %s point (%g %g %g) outside [%g %g %g]..[%g %g %g]\n",
            name, h.pt.x, h.pt.y, h.pt.z, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
        ++g_failed;
    }
}

static void ExpectMiss(const CylinderPairShape& a, const CylinderPairShape& b, const char* name) {
    const Hit h = Query(a, b);
    if (h.hit) {
        std::fprintf(stderr, "FAIL: %s expected no hit (pen %g n %g %g %g)\n",
            name, h.pen, h.n.x, h.n.y, h.n.z);
        ++g_failed;
    }
}

static void ExpectSymmetry(const CylinderPairShape& a, const CylinderPairShape& b, const char* name) {
    const Hit ab = Query(a, b);
    const Hit ba = Query(b, a);
    if (!ab.hit || !ba.hit) {
        std::fprintf(stderr, "FAIL: %s symmetry expected hits both ways\n", name);
        ++g_failed;
        return;
    }
    if (std::fabs(ab.pen - ba.pen) > 2e-3f) {
        std::fprintf(stderr, "FAIL: %s penetration mismatch %g vs %g\n", name, ab.pen, ba.pen);
        ++g_failed;
    }
    const float nd = ab.n.x * ba.n.x + ab.n.y * ba.n.y + ab.n.z * ba.n.z;
    if (nd > -0.98f) {
        std::fprintf(stderr, "FAIL: %s normals not opposite (%g %g %g) vs (%g %g %g)\n",
            name, ab.n.x, ab.n.y, ab.n.z, ba.n.x, ba.n.y, ba.n.z);
        ++g_failed;
    }
    // A line or disc contact can pick different witnesses per order. They still
    // have to lie on the same contact plane (the shared normal).
    const float plane = (ab.pt.x - ba.pt.x) * ab.n.x
        + (ab.pt.y - ba.pt.y) * ab.n.y
        + (ab.pt.z - ba.pt.z) * ab.n.z;
    if (std::fabs(plane) > 0.02f) {
        std::fprintf(stderr, "FAIL: %s contact points not on the same plane (offset %g)\n", name, plane);
        ++g_failed;
    }
}

static void TestBoxCylinder() {
    const CylinderPairShape box = MakeBox(V(0, 0, 0), V(1, 1, 1));
    const CylinderPairShape farCyl = MakeCyl(V(5, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    ExpectMiss(box, farCyl, "box-cyl separated");
    ExpectMiss(farCyl, box, "cyl-box separated");

    // Side: cylinder center x=1.4, r=0.5, left surface x=0.9, box face x=1.
    // Penetration is 0.1. The witness sits in that slab, around x=0.95.
    const CylinderPairShape side = MakeCyl(V(1.4f, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    const Hit sideHit = Query(box, side);
    ExpectHit(sideHit, V(-1, 0, 0), 0.1f, "box-cyl side");
    ExpectPointBox(sideHit, V(0.88f, -1.05f, -0.08f), V(1.02f, 1.05f, 0.08f), "box-cyl side point");
    ExpectSymmetry(box, side, "box-cyl side symmetry");

    // Cap: bottom cap at y=0.8, box top at y=1. Penetration 0.2.
    const CylinderPairShape cap = MakeCyl(V(0, 1.8f, 0), V(0, 1, 0), 0.5f, 1.0f);
    const Hit capHit = Query(box, cap);
    ExpectHit(capHit, V(0, -1, 0), 0.2f, "box-cyl cap");
    ExpectPointBox(capHit, V(-0.55f, 0.82f, -0.55f), V(0.55f, 0.98f, 0.55f), "box-cyl cap point");
    ExpectSymmetry(box, cap, "box-cyl cap symmetry");

    // Rotated cylinder lying on +X, curved side into the box top.
    // Bottom of the barrel at y=0.8. Normal from the box is still -Y.
    const CylinderPairShape rolled = MakeCyl(V(0, 1.3f, 0), V(1, 0, 0), 0.5f, 1.0f);
    const Hit rolledHit = Query(box, rolled);
    ExpectHit(rolledHit, V(0, -1, 0), 0.2f, "box-cyl rotated side");
    ExpectPointBox(rolledHit, V(-1.05f, 0.82f, -0.55f), V(1.05f, 0.98f, 0.55f), "box-cyl rotated side point");

    // Box yawed 45 deg. Support along +X is sqrt(2). Cylinder at x=1.5, r=0.5.
    // pen = sqrt(2) + 0.5 - 1.5.
    const float yawPen = 1.41421356237f + 0.5f - 1.5f;
    const CylinderPairShape yawBox = MakeBoxYaw45(V(0, 0, 0), V(1, 1, 1));
    const CylinderPairShape yawCyl = MakeCyl(V(1.5f, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    const Hit yawHit = Query(yawBox, yawCyl);
    Expect(yawHit.hit, "yawed box vs cylinder hits");
    if (yawHit.hit) {
        Expect(yawHit.n.x < -0.98f && std::fabs(yawHit.n.y) < 0.05f && std::fabs(yawHit.n.z) < 0.05f,
            "yawed box normal is -X");
        if (std::fabs(yawHit.pen - yawPen) > 5e-3f) {
            std::fprintf(stderr, "FAIL: yawed box pen %g expected %g\n", yawHit.pen, yawPen);
            ++g_failed;
        }
    }
    ExpectSymmetry(yawBox, yawCyl, "yawed box symmetry");

    // Fully inside: still a hit. Normal is ambiguous, so only the boolean is checked.
    const CylinderPairShape inside = MakeCyl(V(0, 0, 0), V(0, 1, 0), 0.4f, 0.5f);
    const Hit insideHit = Query(box, inside);
    Expect(insideHit.hit && insideHit.pen > 0.2f, "cylinder inside box");

    CylinderPairShape bad = side;
    bad.extents.x = 0.0f;
    ExpectMiss(box, bad, "zero radius");
    bad.extents.x = -0.5f;
    ExpectMiss(box, bad, "negative radius");
    bad = side;
    bad.extents.y = 0.0f;
    ExpectMiss(box, bad, "zero half-height");
    bad.extents.y = -1.0f;
    ExpectMiss(box, bad, "negative half-height");

    const CylinderPairShape tiny = MakeCyl(V(0, 0, 0), V(0, 1, 0), 1.0e-4f, 0.5f);
    const Hit tinyHit = Query(box, tiny);
    Expect(tinyHit.hit && tinyHit.pen > 0.0f, "tiny positive radius still hits");
}

static void TestCylinderCylinder() {
    const CylinderPairShape a = MakeCyl(V(0, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    const CylinderPairShape far = MakeCyl(V(3, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    ExpectMiss(a, far, "cyl-cyl separated");

    // Side by side. Centers 0.8 apart, radii 0.5+0.5, pen 0.2.
    // Witness midpoint x = 0.4 (A surface 0.5, B surface 0.3).
    const CylinderPairShape side = MakeCyl(V(0.8f, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    const Hit sideHit = Query(a, side);
    ExpectHit(sideHit, V(-1, 0, 0), 0.2f, "cyl-cyl side");
    ExpectPointBox(sideHit, V(0.32f, -1.05f, -0.08f), V(0.48f, 1.05f, 0.08f), "cyl-cyl side point");
    ExpectSymmetry(a, side, "cyl-cyl side symmetry");

    // Different radii: 0.5 + 1.0 - 1.2 = 0.3. Overlap slab x in [0.2, 0.5].
    const CylinderPairShape fat = MakeCyl(V(1.2f, 0, 0), V(0, 1, 0), 1.0f, 1.0f);
    const Hit fatHit = Query(a, fat);
    ExpectHit(fatHit, V(-1, 0, 0), 0.3f, "cyl-cyl unequal side");
    ExpectPointBox(fatHit, V(0.25f, -1.05f, -0.08f), V(0.45f, 1.05f, 0.08f), "cyl-cyl unequal point");

    // Crossed axes: B lies along X at z=0.8. Axis distance 0.8, pen 0.2.
    const CylinderPairShape cross = MakeCyl(V(0, 0, 0.8f), V(1, 0, 0), 0.5f, 1.0f);
    const Hit crossHit = Query(a, cross);
    ExpectHit(crossHit, V(0, 0, -1), 0.2f, "cyl-cyl rotated side");
    ExpectPointBox(crossHit, V(-0.15f, -0.15f, 0.32f), V(0.15f, 0.15f, 0.48f), "cyl-cyl rotated point");
    ExpectSymmetry(a, cross, "cyl-cyl rotated symmetry");

    // Cap to cap. A top y=1, B bottom y=0.7, pen 0.3. Plane near y=0.85.
    const CylinderPairShape cap = MakeCyl(V(0, 1.7f, 0), V(0, 1, 0), 0.4f, 1.0f);
    const Hit capHit = Query(a, cap);
    ExpectHit(capHit, V(0, -1, 0), 0.3f, "cyl-cyl cap");
    ExpectPointBox(capHit, V(-0.45f, 0.78f, -0.45f), V(0.45f, 0.92f, 0.45f), "cyl-cyl cap point");
    ExpectSymmetry(a, cap, "cyl-cyl cap symmetry");

    CylinderPairShape bad = side;
    bad.extents.x = 0.0f;
    ExpectMiss(a, bad, "cyl-cyl zero radius");
    bad.extents.x = -2.0f;
    ExpectMiss(a, bad, "cyl-cyl negative radius");
    bad = side;
    bad.extents.y = 0.0f;
    ExpectMiss(a, bad, "cyl-cyl zero height");
    bad.extents.y = -0.25f;
    ExpectMiss(a, bad, "cyl-cyl negative height");

    const CylinderPairShape needle = MakeCyl(V(0, 0, 0), V(0, 1, 0), 0.2f, 1.0e-4f);
    Expect(Query(a, needle).hit, "tiny positive height still hits");
}

static void TestTriangleCylinder() {
    // Floor covering the origin, and a wall at x=0 covering the origin.
    const CylinderPairShape floor = MakeTri(V(-2, 0, -2), V(2, 0, -2), V(0, 0, 2));
    const CylinderPairShape wall = MakeTri(V(0, -2, -2), V(0, -2, 2), V(0, 2, 0));

    const CylinderPairShape far = MakeCyl(V(5, 1, 0), V(0, 1, 0), 0.5f, 1.0f);
    ExpectMiss(far, floor, "tri-cyl separated");
    ExpectMiss(floor, far, "floor-cyl separated");

    // Cap into the floor. Bottom cap at y=-0.2, pen 0.2. Push cylinder up.
    // Witness on the cap is y=-0.2, on the triangle y=0. Midpoint y=-0.1.
    const CylinderPairShape cap = MakeCyl(V(0, 0.8f, 0), V(0, 1, 0), 0.4f, 1.0f);
    const Hit capHit = Query(cap, floor);
    ExpectHit(capHit, V(0, 1, 0), 0.2f, "tri-cyl cap");
    ExpectPointBox(capHit, V(-0.45f, -0.18f, -0.45f), V(0.45f, 0.02f, 0.45f), "tri-cyl cap point");
    ExpectSymmetry(cap, floor, "tri-cyl cap symmetry");

    // Side into the wall. Right surface x=0.2, wall x=0, pen 0.2. Push cylinder -X.
    const CylinderPairShape side = MakeCyl(V(-0.3f, 0, 0), V(0, 1, 0), 0.5f, 1.0f);
    const Hit sideHit = Query(side, wall);
    ExpectHit(sideHit, V(-1, 0, 0), 0.2f, "tri-cyl side");
    ExpectPointBox(sideHit, V(0.02f, -1.05f, -0.55f), V(0.18f, 1.05f, 0.55f), "tri-cyl side point");
    ExpectSymmetry(side, wall, "tri-cyl side symmetry");

    // Rotated cylinder: axis along +X, center x=-0.7, half-height 1.
    // The +X cap sits at x=0.3, so it has crossed the wall at x=0 by 0.3.
    const CylinderPairShape rotCap = MakeCyl(V(-0.7f, 0, 0), V(1, 0, 0), 0.4f, 1.0f);
    const Hit rotHit = Query(rotCap, wall);
    ExpectHit(rotHit, V(-1, 0, 0), 0.3f, "tri-cyl rotated cap");
    ExpectPointBox(rotHit, V(-0.05f, -0.45f, -0.45f), V(0.35f, 0.45f, 0.45f), "tri-cyl rotated cap point");
    ExpectSymmetry(rotCap, wall, "tri-cyl rotated cap symmetry");

    // Overlapping axis-aligned barrel crossing the floor from above, side not involved.
    // Already covered by cap. A second shallow overlap confirms the boolean both ways.
    const CylinderPairShape shallow = MakeCyl(V(0, 0.95f, 0), V(0, 1, 0), 0.3f, 1.0f);
    Expect(Query(shallow, floor).hit && Query(floor, shallow).hit, "tri-cyl shallow overlap both ways");

    CylinderPairShape bad = cap;
    bad.extents.x = 0.0f;
    ExpectMiss(bad, floor, "tri-cyl zero radius");
    bad.extents.x = -0.4f;
    ExpectMiss(bad, floor, "tri-cyl negative radius");
    bad = cap;
    bad.extents.y = 0.0f;
    ExpectMiss(bad, floor, "tri-cyl zero height");
    bad.extents.y = -1.0f;
    ExpectMiss(bad, floor, "tri-cyl negative height");
}

int main() {
    TestBoxCylinder();
    TestCylinderCylinder();
    TestTriangleCylinder();
    if (g_failed) {
        std::fprintf(stderr, "%d cylinder pair check(s) failed.\n", g_failed);
        return 1;
    }
    std::printf("Cylinder pair checks passed.\n");
    return 0;
}
