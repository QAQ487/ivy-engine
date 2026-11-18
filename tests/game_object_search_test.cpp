// game_object_search_test.cpp
// Linux-runnable unit tests for engine/ecs/game_object_search.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/game_object_search_test tests/game_object_search_test.cpp
//   /tmp/game_object_search_test

#include "engine/ecs/game_object_search.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

// Stand-in for GameObject: name, active flag, owned children.
struct Node {
    std::string name;
    bool active = true;
    std::vector<std::unique_ptr<Node>> children;

    Node* Add(const std::string& childName, bool childActive = true) {
        children.push_back(std::make_unique<Node>());
        children.back()->name = childName;
        children.back()->active = childActive;
        return children.back().get();
    }
};

using Roots = std::vector<std::unique_ptr<Node>>;
using game_object_search::Visit;

static const std::vector<std::unique_ptr<Node>>& Kids(Node& n) { return n.children; }

static std::string Order(const Roots& roots, bool skipInactive) {
    std::string out;
    game_object_search::WalkDepthFirst<Node>(roots, Kids, [&](Node& n) {
        if (skipInactive && !n.active) return Visit::SkipChildren;
        out += n.name;
        return Visit::Continue;
    });
    return out;
}

int main() {
    // A(B(C,D),E) F(G)   + a null root in between
    Roots roots;
    roots.push_back(std::make_unique<Node>());
    roots.back()->name = "A";
    Node* b = roots.back()->Add("B");
    b->Add("C");
    b->Add("D");
    roots.back()->Add("E");
    roots.push_back(nullptr);
    roots.push_back(std::make_unique<Node>());
    roots.back()->name = "F";
    roots.back()->Add("G");

    Expect(Order(roots, false) == "ABCDEFG", "pre-order, scene order, null root skipped");

    // Inactive subtree skipped as a whole.
    b->active = false;
    Expect(Order(roots, true) == "AEFG", "inactive B hides C and D");
    Expect(Order(roots, false) == "ABCDEFG", "includeInactive walks everything");
    b->active = true;

    // Stop ends the walk and the function reports it.
    std::string seen;
    const bool finished = game_object_search::WalkDepthFirst<Node>(roots, Kids, [&](Node& n) {
        seen += n.name;
        return n.name == "D" ? Visit::Stop : Visit::Continue;
    });
    Expect(!finished && seen == "ABCD", "stop at D");

    // First match by name is the first in scene order (duplicate names).
    roots.back()->Add("C");  // second "C" under F
    Node* firstC = nullptr;
    game_object_search::WalkDepthFirst<Node>(roots, Kids, [&](Node& n) {
        if (n.name == "C") { firstC = &n; return Visit::Stop; }
        return Visit::Continue;
    });
    Expect(firstC == b->children[0].get(), "first C is the one under B");

    // Null child pointers are skipped.
    b->children.insert(b->children.begin() + 1, nullptr);
    Expect(Order(roots, false) == "ABCDEFGC", "null child skipped");

    // Deep hierarchy: iterative walk does not recurse.
    Roots deep;
    deep.push_back(std::make_unique<Node>());
    deep.back()->name = "x";
    Node* cur = deep.back().get();
    for (int i = 0; i < 200000; ++i)
        cur = cur->Add("x");
    int count = 0;
    game_object_search::WalkDepthFirst<Node>(deep, Kids, [&](Node&) { ++count; return Visit::Continue; });
    Expect(count == 200001, "200k-deep chain walked without recursion");
    // Tear the chain down iteratively so unique_ptr destructors do not recurse.
    std::vector<std::unique_ptr<Node>> pending;
    for (auto& r : deep) pending.push_back(std::move(r));
    while (!pending.empty()) {
        std::unique_ptr<Node> n = std::move(pending.back());
        pending.pop_back();
        for (auto& c : n->children) pending.push_back(std::move(c));
    }

    // Empty roots.
    Roots none;
    Expect(game_object_search::WalkDepthFirst<Node>(none, Kids, [](Node&) { return Visit::Continue; }),
           "empty roots finish");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("game_object_search_test: all checks passed\n");
    return 0;
}
