// game_object_search.h
// Pure depth-first search over a scene tree, used by the script API's
// find-by-name / find-by-tag. Templated on the node type so it is
// unit-testable on Linux with a stand-in node (no GameObject dependency).

#pragma once

#include <iterator>
#include <vector>

namespace game_object_search {

enum class Visit {
    Continue,      // look at this node's children next
    SkipChildren,  // do not descend into this node
    Stop,          // end the search
};

// Pre-order depth-first walk over roots (scene order, parent before children).
// children(node) returns an iterable of child pointers (raw or smart);
// visit(node) returns a Visit. Null nodes are skipped. Iterative, so a deep
// hierarchy cannot overflow the stack. Returns false if visit asked to stop.
template <class Node, class RootRange, class ChildrenFn, class VisitFn>
bool WalkDepthFirst(const RootRange& roots, ChildrenFn children, VisitFn visit) {
    std::vector<Node*> stack;
    for (auto it = std::rbegin(roots); it != std::rend(roots); ++it) {
        if (*it)
            stack.push_back(&**it);
    }
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        const Visit v = visit(*node);
        if (v == Visit::Stop)
            return false;
        if (v == Visit::SkipChildren)
            continue;
        const auto& kids = children(*node);
        for (auto it = std::rbegin(kids); it != std::rend(kids); ++it) {
            if (*it)
                stack.push_back(&**it);
        }
    }
    return true;
}

}  // namespace game_object_search
