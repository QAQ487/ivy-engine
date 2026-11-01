#include "cylinder_pair.h"

#include <algorithm>
#include <cmath>

namespace {

struct Vec {
    float x, y, z;
};

static Vec V(float x, float y, float z) { return { x, y, z }; }

static Vec Add(Vec a, Vec b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static Vec Sub(Vec a, Vec b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static Vec Mul(Vec a, float s) { return { a.x * s, a.y * s, a.z * s }; }
static float Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec Cross(Vec a, Vec b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
static float LenSq(Vec a) { return Dot(a, a); }
static float Len(Vec a) { return std::sqrt(LenSq(a)); }

static Vec FromPair(const CylinderPairVec3& v) { return { v.x, v.y, v.z }; }
static CylinderPairVec3 ToPair(Vec v) { return { v.x, v.y, v.z }; }

static bool FiniteVec(Vec v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static Vec NormalizeOr(Vec v, Vec fallback) {
    const float len = Len(v);
    if (!(len > 1e-8f) || !FiniteVec(v))
        return fallback;
    return Mul(v, 1.0f / len);
}

static float SignNonZero(float v) {
    if (v > 1e-8f) return 1.0f;
    if (v < -1e-8f) return -1.0f;
    return 0.0f;
}

struct Shape {
    CylinderPairShapeKind kind;
    Vec center;
    Vec axisX;
    Vec axisY;
    Vec axisZ;
    Vec extents;
    Vec tri1;
    Vec tri2;
};

static Shape LoadShape(const CylinderPairShape& in) {
    Shape s;
    s.kind = in.kind;
    s.center = FromPair(in.center);
    s.axisX = FromPair(in.axisX);
    s.axisY = FromPair(in.axisY);
    s.axisZ = FromPair(in.axisZ);
    s.extents = FromPair(in.extents);
    s.tri1 = FromPair(in.tri1);
    s.tri2 = FromPair(in.tri2);
    if (s.kind == CylinderPairShapeKind::Cylinder)
        s.axisY = NormalizeOr(s.axisY, V(0.0f, 1.0f, 0.0f));
    if (s.kind == CylinderPairShapeKind::Box) {
        s.axisX = NormalizeOr(s.axisX, V(1.0f, 0.0f, 0.0f));
        s.axisY = NormalizeOr(s.axisY, V(0.0f, 1.0f, 0.0f));
        s.axisZ = NormalizeOr(s.axisZ, V(0.0f, 0.0f, 1.0f));
    }
    return s;
}

static bool CylinderRejected(const Shape& s) {
    if (s.kind != CylinderPairShapeKind::Cylinder)
        return false;
    // radius and half-height must be strictly positive and finite.
    if (!(s.extents.x > 0.0f) || !(s.extents.y > 0.0f))
        return true;
    if (!std::isfinite(s.extents.x) || !std::isfinite(s.extents.y))
        return true;
    return false;
}

static Vec ShapeCenter(const Shape& s) {
    if (s.kind == CylinderPairShapeKind::Triangle)
        return Mul(Add(Add(s.center, s.tri1), s.tri2), 1.0f / 3.0f);
    return s.center;
}

static Vec SupportShape(const Shape& s, Vec dir) {
    switch (s.kind) {
    case CylinderPairShapeKind::Box: {
        Vec r = s.center;
        r = Add(r, Mul(s.axisX, s.extents.x * SignNonZero(Dot(dir, s.axisX))));
        r = Add(r, Mul(s.axisY, s.extents.y * SignNonZero(Dot(dir, s.axisY))));
        r = Add(r, Mul(s.axisZ, s.extents.z * SignNonZero(Dot(dir, s.axisZ))));
        return r;
    }
    case CylinderPairShapeKind::Cylinder: {
        const float axial = Dot(dir, s.axisY);
        Vec r = Add(s.center, Mul(s.axisY, s.extents.y * SignNonZero(axial)));
        const Vec radial = Sub(dir, Mul(s.axisY, axial));
        const float radialLen = Len(radial);
        if (radialLen > 1e-8f)
            r = Add(r, Mul(radial, s.extents.x / radialLen));
        return r;
    }
    case CylinderPairShapeKind::Triangle: {
        const float d0 = Dot(s.center, dir);
        const float d1 = Dot(s.tri1, dir);
        const float d2 = Dot(s.tri2, dir);
        if (d0 >= d1 && d0 >= d2) return s.center;
        if (d1 >= d2) return s.tri1;
        return s.tri2;
    }
    }
    return s.center;
}

struct Sup {
    Vec p;
    Vec a;
    Vec b;
};

static Sup Support(const Shape& a, const Shape& b, Vec dir) {
    Sup s;
    s.a = SupportShape(a, dir);
    s.b = SupportShape(b, Mul(dir, -1.0f));
    s.p = Sub(s.a, s.b);
    return s;
}

struct Simplex {
    Sup v[4];
    int n = 0;
};

static bool SameDir(Vec a, Vec b) { return Dot(a, b) > 0.0f; }

static bool EvolveLine(Simplex& s, Vec& dir) {
    const Vec a = s.v[0].p;
    const Vec b = s.v[1].p;
    const Vec ab = Sub(b, a);
    const Vec ao = Mul(a, -1.0f);
    if (SameDir(ab, ao)) {
        dir = Cross(Cross(ab, ao), ab);
    } else {
        s.n = 1;
        dir = ao;
    }
    return false;
}

static bool EvolveTriangle(Simplex& s, Vec& dir) {
    const Sup a = s.v[0];
    const Sup b = s.v[1];
    const Sup c = s.v[2];
    const Vec ab = Sub(b.p, a.p);
    const Vec ac = Sub(c.p, a.p);
    const Vec ao = Mul(a.p, -1.0f);
    const Vec abc = Cross(ab, ac);

    if (SameDir(Cross(abc, ac), ao)) {
        if (SameDir(ac, ao)) {
            s.v[0] = a;
            s.v[1] = c;
            s.n = 2;
            dir = Cross(Cross(ac, ao), ac);
        } else {
            s.v[0] = a;
            s.v[1] = b;
            s.n = 2;
            return EvolveLine(s, dir);
        }
    } else if (SameDir(Cross(ab, abc), ao)) {
        s.v[0] = a;
        s.v[1] = b;
        s.n = 2;
        return EvolveLine(s, dir);
    } else if (SameDir(abc, ao)) {
        dir = abc;
    } else {
        s.v[0] = a;
        s.v[1] = c;
        s.v[2] = b;
        s.n = 3;
        dir = Mul(abc, -1.0f);
    }
    return false;
}

static bool EvolveTetra(Simplex& s, Vec& dir) {
    const Sup a = s.v[0];
    const Sup b = s.v[1];
    const Sup c = s.v[2];
    const Sup d = s.v[3];
    const Vec ao = Mul(a.p, -1.0f);
    const Vec abc = Cross(Sub(b.p, a.p), Sub(c.p, a.p));
    const Vec acd = Cross(Sub(c.p, a.p), Sub(d.p, a.p));
    const Vec adb = Cross(Sub(d.p, a.p), Sub(b.p, a.p));

    if (SameDir(abc, ao)) {
        s.v[0] = a;
        s.v[1] = b;
        s.v[2] = c;
        s.n = 3;
        return EvolveTriangle(s, dir);
    }
    if (SameDir(acd, ao)) {
        s.v[0] = a;
        s.v[1] = c;
        s.v[2] = d;
        s.n = 3;
        return EvolveTriangle(s, dir);
    }
    if (SameDir(adb, ao)) {
        s.v[0] = a;
        s.v[1] = d;
        s.v[2] = b;
        s.n = 3;
        return EvolveTriangle(s, dir);
    }
    return true;
}

static bool Evolve(Simplex& s, Vec& dir) {
    switch (s.n) {
    case 2: return EvolveLine(s, dir);
    case 3: return EvolveTriangle(s, dir);
    case 4: return EvolveTetra(s, dir);
    default:
        dir = Mul(s.v[0].p, -1.0f);
        return false;
    }
}

static bool GjkOverlap(const Shape& a, const Shape& b, Simplex& out) {
    Vec dir = NormalizeOr(Sub(ShapeCenter(a), ShapeCenter(b)), V(1.0f, 0.0f, 0.0f));
    Simplex s;
    s.n = 1;
    s.v[0] = Support(a, b, dir);
    dir = NormalizeOr(Mul(s.v[0].p, -1.0f), V(-1.0f, 0.0f, 0.0f));

    for (int iter = 0; iter < 64; ++iter) {
        const Sup next = Support(a, b, dir);
        // Support did not reach the origin: the shapes are separated.
        if (Dot(next.p, dir) < -1e-6f)
            return false;

        if (s.n >= 4) {
            out = s;
            return true;
        }
        for (int i = s.n; i > 0; --i)
            s.v[i] = s.v[i - 1];
        s.v[0] = next;
        if (s.n < 4)
            ++s.n;

        if (Evolve(s, dir)) {
            out = s;
            return true;
        }
        if (LenSq(dir) < 1e-16f) {
            out = s;
            return true;
        }
        dir = NormalizeOr(dir, V(0.0f, 1.0f, 0.0f));
    }
    out = s;
    return false;
}

struct Face {
    int i0, i1, i2;
    Vec n;
    float dist;
};

static bool MakeFace(const Sup* verts, int i0, int i1, int i2, Face& out) {
    Vec n = Cross(Sub(verts[i1].p, verts[i0].p), Sub(verts[i2].p, verts[i0].p));
    const float len = Len(n);
    if (!(len > 1e-10f))
        return false;
    n = Mul(n, 1.0f / len);
    float dist = Dot(n, verts[i0].p);
    if (dist < 0.0f) {
        std::swap(i1, i2);
        n = Mul(n, -1.0f);
        dist = -dist;
    }
    out.i0 = i0;
    out.i1 = i1;
    out.i2 = i2;
    out.n = n;
    out.dist = dist;
    return true;
}

struct Edge {
    int a;
    int b;
};

static void PushHorizon(Edge* edges, int& count, int a, int b) {
    for (int i = 0; i < count; ++i) {
        if (edges[i].a == b && edges[i].b == a) {
            edges[i] = edges[--count];
            return;
        }
    }
    edges[count++] = { a, b };
}

static bool Barycentric(Vec p0, Vec p1, Vec p2, Vec q, float& u, float& v, float& w) {
    const Vec e0 = Sub(p1, p0);
    const Vec e1 = Sub(p2, p0);
    const Vec e2 = Sub(q, p0);
    const float d00 = Dot(e0, e0);
    const float d01 = Dot(e0, e1);
    const float d11 = Dot(e1, e1);
    const float d20 = Dot(e2, e0);
    const float d21 = Dot(e2, e1);
    const float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) < 1e-12f)
        return false;
    v = (d11 * d20 - d01 * d21) / denom;
    w = (d00 * d21 - d01 * d20) / denom;
    u = 1.0f - v - w;
    return true;
}

static bool EpaContact(const Shape& shapeA, const Shape& shapeB, Simplex simplex,
    Vec& outNormal, float& outPen, Vec& outPoint) {
    constexpr int kMaxVerts = 64;
    constexpr int kMaxFaces = 128;
    Sup verts[kMaxVerts];
    int vertCount = simplex.n;
    if (vertCount > 4)
        vertCount = 4;
    for (int i = 0; i < vertCount; ++i)
        verts[i] = simplex.v[i];

    // A 1-3 point simplex that contains the origin is only a touch. Search a
    // few extra supports so EPA has a volume when the overlap is real.
    if (vertCount < 4) {
        const Vec seeds[] = {
            V(1, 0, 0), V(-1, 0, 0), V(0, 1, 0), V(0, -1, 0), V(0, 0, 1), V(0, 0, -1),
            V(1, 1, 1), V(-1, 1, -1), V(1, -1, 1), V(-1, -1, 1)
        };
        for (Vec seed : seeds) {
            if (vertCount >= 4)
                break;
            const Sup extra = Support(shapeA, shapeB, NormalizeOr(seed, seed));
            bool dup = false;
            for (int i = 0; i < vertCount; ++i) {
                if (LenSq(Sub(extra.p, verts[i].p)) < 1e-10f)
                    dup = true;
            }
            if (!dup && vertCount < kMaxVerts)
                verts[vertCount++] = extra;
        }
    }
    if (vertCount < 4)
        return false;

    Face faces[kMaxFaces];
    int faceCount = 0;
    const int seed[4][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 1 }, { 1, 3, 2 } };
    for (int f = 0; f < 4; ++f) {
        Face face;
        if (MakeFace(verts, seed[f][0], seed[f][1], seed[f][2], face) && faceCount < kMaxFaces)
            faces[faceCount++] = face;
    }
    if (faceCount < 4)
        return false;

    constexpr float kSlop = 1e-5f;
    int best = 0;
    for (int iter = 0; iter < 64; ++iter) {
        best = 0;
        for (int i = 1; i < faceCount; ++i) {
            if (faces[i].dist < faces[best].dist)
                best = i;
        }
        const Face& face = faces[best];
        const Sup next = Support(shapeA, shapeB, face.n);
        const float proj = Dot(next.p, face.n);
        if (proj - face.dist < kSlop || vertCount >= kMaxVerts || faceCount >= kMaxFaces - 8)
            break;

        Edge horizon[kMaxFaces * 3];
        int horizonCount = 0;
        bool remove[kMaxFaces];
        for (int i = 0; i < faceCount; ++i) {
            remove[i] = Dot(faces[i].n, next.p) > faces[i].dist + 1e-6f;
            if (!remove[i])
                continue;
            PushHorizon(horizon, horizonCount, faces[i].i0, faces[i].i1);
            PushHorizon(horizon, horizonCount, faces[i].i1, faces[i].i2);
            PushHorizon(horizon, horizonCount, faces[i].i2, faces[i].i0);
        }

        int write = 0;
        for (int i = 0; i < faceCount; ++i) {
            if (!remove[i])
                faces[write++] = faces[i];
        }
        faceCount = write;

        const int newIndex = vertCount++;
        verts[newIndex] = next;
        for (int e = 0; e < horizonCount; ++e) {
            if (faceCount >= kMaxFaces)
                break;
            Face grown;
            if (MakeFace(verts, horizon[e].a, horizon[e].b, newIndex, grown))
                faces[faceCount++] = grown;
        }
        if (faceCount == 0)
            return false;
    }

    best = 0;
    for (int i = 1; i < faceCount; ++i) {
        if (faces[i].dist < faces[best].dist)
            best = i;
    }
    const Face& face = faces[best];
    if (!(face.dist >= -1e-4f) || !FiniteVec(face.n))
        return false;

    // face.n points from the origin toward the closest boundary of A-B.
    // Translating A by -face.n * dist moves that boundary onto the origin.
    outNormal = Mul(face.n, -1.0f);
    outPen = (std::max)(face.dist, 0.0f);

    const Vec q = Mul(face.n, face.dist);
    float u = 1.0f, v = 0.0f, w = 0.0f;
    if (!Barycentric(verts[face.i0].p, verts[face.i1].p, verts[face.i2].p, q, u, v, w)) {
        u = 1.0f; v = 0.0f; w = 0.0f;
    }
    u = (std::max)(0.0f, (std::min)(1.0f, u));
    v = (std::max)(0.0f, (std::min)(1.0f, v));
    w = (std::max)(0.0f, (std::min)(1.0f, w));
    const float sum = u + v + w;
    if (sum > 1e-8f) {
        u /= sum; v /= sum; w /= sum;
    } else {
        u = 1.0f; v = 0.0f; w = 0.0f;
    }
    const Vec pointA = Add(Add(Mul(verts[face.i0].a, u), Mul(verts[face.i1].a, v)), Mul(verts[face.i2].a, w));
    const Vec pointB = Add(Add(Mul(verts[face.i0].b, u), Mul(verts[face.i1].b, v)), Mul(verts[face.i2].b, w));
    outPoint = Mul(Add(pointA, pointB), 0.5f);
    return FiniteVec(outNormal) && std::isfinite(outPen) && FiniteVec(outPoint);
}

} // namespace

bool CylinderPair_Contact(const CylinderPairShape& aIn,
    const CylinderPairShape& bIn,
    CylinderPairVec3& outNormal,
    float& outPenetration,
    CylinderPairVec3* outPoint) {
    outNormal = {};
    outPenetration = 0.0f;
    if (outPoint)
        *outPoint = {};

    const Shape a = LoadShape(aIn);
    const Shape b = LoadShape(bIn);
    if (CylinderRejected(a) || CylinderRejected(b))
        return false;

    Simplex simplex;
    if (!GjkOverlap(a, b, simplex))
        return false;

    Vec normal, point;
    float pen = 0.0f;
    if (!EpaContact(a, b, simplex, normal, pen, point))
        return false;
    if (!(pen >= 0.0f))
        return false;

    const float nlen = Len(normal);
    if (!(nlen > 1e-8f))
        return false;
    normal = Mul(normal, 1.0f / nlen);

    outNormal = ToPair(normal);
    outPenetration = pen;
    if (outPoint)
        *outPoint = ToPair(point);
    return true;
}
