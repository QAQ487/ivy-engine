#include "PlayerAnimTestScript.h"

#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ui/ui_canvas.h"
#include "engine/ui/ui_text.h"

#include <cstdio>
#include <string>

REGISTER_SCRIPT(PlayerAnimTestScript)

void PlayerAnimTestScript::SetupClips(engine::GameObject& self) {
    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    if (!vc) return;

    vc->playAnimation = true;
    vc->animationPaths[idleClip] = idlePath;
    vc->animationPaths[runClip] = runPath;
    vc->animationPaths[danceClip] = dancePath;
    vc->animationPaths[jumpClip] = jumpPath;
    vc->animationPaths[pushClip] = pushPath;
    vc->animationPaths[pullClip] = pullPath;
    vc->animationPaths[throwClip] = throwPath;
}

void PlayerAnimTestScript::PlayNamedClip(engine::GameObject& self, const std::string& clipName) {
    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    if (!vc || !vc->playAnimation) return;

    m_lastRequestedClip = clipName;
    ScriptApi_Visual_PlayClip(vc, clipName.c_str());
    SyncHud(self);
}

void PlayerAnimTestScript::BindHud() {
    if (m_statusText) return;
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas) return;
    if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, "AnimTestStatus"))
        m_statusText = dynamic_cast<UIText*>(element);
}

void PlayerAnimTestScript::SyncHud(engine::GameObject& self) {
    BindHud();
    if (!m_statusText) return;

    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    const std::string active = vc ? vc->activeClip : std::string();
    const bool hasPath = vc && vc->animationPaths.find(m_lastRequestedClip) != vc->animationPaths.end();

    char buf[512];
    std::snprintf(
        buf,
        sizeof(buf),
        "Player Anim Test\n"
        "1 idle  2 run  3 dance  4 jump\n"
        "5 push  6 pull  7 throw  0 base/T-pose\n"
        "Q/E yaw\n"
        "requested: %s\n"
        "activeClip: %s\n"
        "path registered: %s",
        m_lastRequestedClip.empty() ? "(base)" : m_lastRequestedClip.c_str(),
        active.empty() ? "(base/empty)" : active.c_str(),
        m_lastRequestedClip.empty() ? "n/a" : (hasPath ? "yes" : "NO"));
    ScriptApi_UITextSetText(*m_statusText, buf);
}

void PlayerAnimTestScript::OnCreate(engine::GameObject& self) {
    SetupClips(self);
    m_lastRequestedClip = idleClip;
    PlayNamedClip(self, idleClip);
    BindHud();
    SyncHud(self);
}

void PlayerAnimTestScript::OnUpdate(engine::GameObject& self, float dt) {
    if (KeyLogger_IsTrigger(KK_D1))
        PlayNamedClip(self, idleClip);
    else if (KeyLogger_IsTrigger(KK_D2))
        PlayNamedClip(self, runClip);
    else if (KeyLogger_IsTrigger(KK_D3))
        PlayNamedClip(self, danceClip);
    else if (KeyLogger_IsTrigger(KK_D4))
        PlayNamedClip(self, jumpClip);
    else if (KeyLogger_IsTrigger(KK_D5))
        PlayNamedClip(self, pushClip);
    else if (KeyLogger_IsTrigger(KK_D6))
        PlayNamedClip(self, pullClip);
    else if (KeyLogger_IsTrigger(KK_D7))
        PlayNamedClip(self, throwClip);
    else if (KeyLogger_IsTrigger(KK_D0))
        PlayNamedClip(self, "");

    float yawDelta = 0.0f;
    if (const Keyboard_State* kb = Keyboard_GetState()) {
        if (kb->Q)
            yawDelta -= spinSpeed * dt;
        if (kb->E)
            yawDelta += spinSpeed * dt;
    }

    if (yawDelta != 0.0f) {
        self.m_rotation.y += yawDelta;
        if (engine::Transform* t = ScriptApi_FindTransform(self))
            t->localRotation.y = self.m_rotation.y;
    }

    SyncHud(self);
}
