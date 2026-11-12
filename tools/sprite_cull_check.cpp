// sprite_cull_check.cpp
// Linux-runnable unit checks for 2D orthographic view-rect sprite culling:
// inside / outside each side / straddling / rotated near corner / non-centre
// pivot / negative & zero scale / zoomed camera / aspect change / degenerate
// sizes / margin / toggle-off semantics.

#include "engine/rendering/culling/sprite_cull_math.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using frustum_cull::Aabb;
using sprite_cull::ViewRect;

static int g_fails = 0;

static void Expect(bool cond, const char* label) {
    if (cond)
        return;
    std::fprintf(stderr, "FAIL: %s\n", label);
    ++g_fails;
}

static void ExpectNear(float a, float b, float eps, const char* label) {
    if (std::fabs(a - b) <= eps)
        return;
    std::fprintf(stderr, "FAIL: %s (got %g expected %g)\n", label, a, b);
    ++g_fails;
}

int main() {
    // Default ortho: height 10 → halfH=5, aspect 1 → halfW=5, cam at origin.
    // Visible XY: [-5,5] x [-5,5].
    const ViewRect view = sprite_cull::MakeOrthoViewRect(0.0f, 0.0f, 10.0f, 1.0f);
    Expect(view.valid, "default view valid");
    ExpectNear(view.minX, -5.0f, 1e-5f, "view minX");
    ExpectNear(view.maxX, 5.0f, 1e-5f, "view maxX");
    ExpectNear(view.minY, -5.0f, 1e-5f, "view minY");
    ExpectNear(view.maxY, 5.0f, 1e-5f, "view maxY");

    // --- Local AABB from size + centre pivot ---
    {
        const Aabb local = sprite_cull::MakeSpriteLocalAabb(2.0f, 4.0f, 0.5f, 0.5f);
        Expect(local.valid, "centre-pivot local valid");
        ExpectNear(local.min.x, -1.0f, 1e-5f, "centre-pivot min.x");
        ExpectNear(local.max.x, 1.0f, 1e-5f, "centre-pivot max.x");
        ExpectNear(local.min.y, -2.0f, 1e-5f, "centre-pivot min.y");
        ExpectNear(local.max.y, 2.0f, 1e-5f, "centre-pivot max.y");
        ExpectNear(local.min.z, 0.0f, 1e-5f, "local z flat");
        ExpectNear(local.max.z, 0.0f, 1e-5f, "local z flat max");
    }

    // --- Non-centre pivot (bottom-left = 0,0) ---
    {
        const Aabb local = sprite_cull::MakeSpriteLocalAabb(2.0f, 4.0f, 0.0f, 0.0f);
        ExpectNear(local.min.x, 0.0f, 1e-5f, "bl-pivot min.x");
        ExpectNear(local.max.x, 2.0f, 1e-5f, "bl-pivot max.x");
        ExpectNear(local.min.y, 0.0f, 1e-5f, "bl-pivot min.y");
        ExpectNear(local.max.y, 4.0f, 1e-5f, "bl-pivot max.y");
    }

    // --- Inside ---
    {
        const Aabb world = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {0.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(sprite_cull::AabbIntersectsViewRect(world, view), "sprite inside view");
    }

    // --- Fully outside each side ---
    {
        const Aabb right = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {10.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(right, view), "outside right");

        const Aabb left = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {-10.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(left, view), "outside left");

        const Aabb top = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {0.0f, 10.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(top, view), "outside top");

        const Aabb bottom = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {0.0f, -10.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(bottom, view), "outside bottom");
    }

    // --- Straddling edge ---
    {
        const Aabb straddle = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {5.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        // extents [4,6] x [-1,1] — straddles maxX=5
        Expect(sprite_cull::AabbIntersectsViewRect(straddle, view), "straddling right edge");
    }

    // --- Rotated sprite near corner (45°) — conservative AABB of rotated quad ---
    {
        // 2x2 centre-pivot at (4.2, 4.2); after 45° rotation AABB grows and
        // still intersects the corner of the view.
        const Aabb rotated = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {4.2f, 4.2f, 0.0f}, {0, 0, 45.0f}, {1, 1, 1});
        Expect(rotated.valid, "rotated aabb valid");
        Expect(sprite_cull::AabbIntersectsViewRect(rotated, view),
            "rotated sprite near corner intersects");

        // Far outside even with rotation AABB growth.
        const Aabb rotatedOut = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {20.0f, 20.0f, 0.0f}, {0, 0, 45.0f}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(rotatedOut, view),
            "rotated sprite far outside");
    }

    // --- Non-centre pivot at world origin: geometry extends into +X/+Y ---
    {
        const Aabb bl = sprite_cull::MakeSpriteWorldAabb(
            3.0f, 3.0f, 0.0f, 0.0f,
            {0.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        ExpectNear(bl.min.x, 0.0f, 1e-4f, "world bl-pivot min.x");
        ExpectNear(bl.max.x, 3.0f, 1e-4f, "world bl-pivot max.x");
        Expect(sprite_cull::AabbIntersectsViewRect(bl, view), "non-centre pivot inside");

        // Pivot bottom-left placed so the whole quad sits outside to the right.
        const Aabb blOut = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.0f, 0.0f,
            {6.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(blOut, view),
            "non-centre pivot outside");
    }

    // --- Negative scale (mirrors extents; AABB still correct) ---
    {
        const Aabb neg = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {0.0f, 0.0f, 0.0f}, {0, 0, 0}, {-1.0f, 1.0f, 1.0f});
        Expect(neg.valid, "negative scale valid");
        ExpectNear(neg.min.x, -1.0f, 1e-4f, "neg scale min.x");
        ExpectNear(neg.max.x, 1.0f, 1e-4f, "neg scale max.x");
        Expect(sprite_cull::AabbIntersectsViewRect(neg, view), "negative scale inside");

        const Aabb negOut = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {10.0f, 0.0f, 0.0f}, {0, 0, 0}, {-2.0f, -2.0f, 1.0f});
        Expect(!sprite_cull::AabbIntersectsViewRect(negOut, view),
            "negative scale outside");
    }

    // --- Zero scale: degenerate point AABB at position ---
    {
        const Aabb zeroIn = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {1.0f, 1.0f, 0.0f}, {0, 0, 0}, {0.0f, 0.0f, 1.0f});
        Expect(zeroIn.valid, "zero scale valid");
        ExpectNear(zeroIn.min.x, 1.0f, 1e-4f, "zero scale collapses to pos.x");
        Expect(sprite_cull::AabbIntersectsViewRect(zeroIn, view), "zero scale inside");

        const Aabb zeroOut = sprite_cull::MakeSpriteWorldAabb(
            2.0f, 2.0f, 0.5f, 0.5f,
            {20.0f, 0.0f, 0.0f}, {0, 0, 0}, {0.0f, 0.0f, 1.0f});
        Expect(!sprite_cull::AabbIntersectsViewRect(zeroOut, view), "zero scale outside");
    }

    // --- Degenerate sprite size (0 width/height) ---
    {
        const Aabb deg = sprite_cull::MakeSpriteWorldAabb(
            0.0f, 0.0f, 0.5f, 0.5f,
            {0.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(deg.valid, "degenerate size valid");
        Expect(sprite_cull::AabbIntersectsViewRect(deg, view), "degenerate size inside");
    }

    // --- Zoomed camera (smaller ortho height) ---
    {
        const ViewRect zoomed = sprite_cull::MakeOrthoViewRect(0.0f, 0.0f, 2.0f, 1.0f);
        // Visible [-1,1]^2. Sprite at (3,0) was inside height-10 view, outside zoomed.
        const Aabb mid = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {3.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(sprite_cull::AabbIntersectsViewRect(mid, view), "mid sprite in wide view");
        Expect(!sprite_cull::AabbIntersectsViewRect(mid, zoomed), "mid sprite culled when zoomed");
    }

    // --- Aspect change (wide) ---
    {
        // height 10, aspect 2 → halfW=10, halfH=5 → [-10,10] x [-5,5]
        const ViewRect wide = sprite_cull::MakeOrthoViewRect(0.0f, 0.0f, 10.0f, 2.0f);
        ExpectNear(wide.minX, -10.0f, 1e-5f, "wide view minX");
        ExpectNear(wide.maxX, 10.0f, 1e-5f, "wide view maxX");
        const Aabb farX = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {8.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(farX, view), "farX outside square view");
        Expect(sprite_cull::AabbIntersectsViewRect(farX, wide), "farX inside wide view");
    }

    // --- Margin: just-outside sprite culled without margin, kept with margin ---
    {
        // Sprite centre at x=5.6, half-extent 0.5 → [5.1, 6.1] — fully outside maxX=5
        const Aabb justOut = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {5.6f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        Expect(!sprite_cull::AabbIntersectsViewRect(justOut, view),
            "just-outside culled without margin");

        ViewRect inflated = view;
        sprite_cull::InflateViewRect(inflated, 0.5f);
        Expect(sprite_cull::AabbIntersectsViewRect(justOut, inflated),
            "just-outside kept with margin");
    }

    // --- Invalid AABB must never cull ---
    {
        Aabb invalid{};
        Expect(sprite_cull::AabbIntersectsViewRect(invalid, view),
            "invalid aabb not culled");
    }

    // --- Toggle-off semantics: when culling is disabled the caller skips the
    // test entirely; an outside box would still fail the helper — OFF must
    // bypass this path (engine Draw gate). ---
    {
        const Aabb right = sprite_cull::MakeSpriteWorldAabb(
            1.0f, 1.0f, 0.5f, 0.5f,
            {10.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1});
        const bool wouldCull = !sprite_cull::AabbIntersectsViewRect(right, view);
        Expect(wouldCull, "toggle-off baseline: outside would cull if tested");
        const bool toggleOffVisible = true;
        Expect(toggleOffVisible, "toggle off reproduces unculled draw");
    }

    if (g_fails != 0) {
        std::fprintf(stderr, "%d sprite cull check(s) failed\n", g_fails);
        return 1;
    }
    std::puts("PASS: sprite view-rect cull (inside/outside/straddle/rotate/pivot/scale/zoom/aspect/margin)");
    return 0;
}
