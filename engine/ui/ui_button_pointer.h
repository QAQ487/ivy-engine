// ui_button_pointer.h
// Pointer/hover hit-testing helpers for UI buttons.

#pragma once

// Pointer-click helper for UIButton. Hidden buttons skip Update, so leftover
// mouse-down state must be cleared in SetVisible or they fire a phantom click
// on the next show (pause Resume then Esc lands on Lobby).

struct UIButtonPointerState {
    bool wasMouseDown = false;
};

inline void UIButtonResetPointerState(UIButtonPointerState& state) {
    state.wasMouseDown = false;
}

inline bool UIButtonPollClick(UIButtonPointerState& state, bool hovered, bool mouseDown) {
    const bool released = !mouseDown && state.wasMouseDown;
    state.wasMouseDown = mouseDown;
    return hovered && released;
}
