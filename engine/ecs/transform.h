/*****************************************************
 * File: transform.h
 * Project: d3d11_engine
 * Author: Terry
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::Transform now lives on ::GameObject (via the Component system).
 *   No API changes – callers that did node->GetComponent<Transform>() still
 *   work identically.
 *****************************************************/
#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

// Forward declaration to avoid including game_object.h here.
class GameObject;

namespace engine {

// Component that stores local-space position, rotation (Euler degrees) and
// scale.  World-space matrix is derived by composing this component's local
// matrix with the chain of parent GameObject transforms.
class Transform : public Component {
public:
    DirectX::XMFLOAT3 localPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f }; // Euler degrees
    DirectX::XMFLOAT3 localScale    = { 1.0f, 1.0f, 1.0f };

    const char* GetTypeName() const override { return "SceneTransform"; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void OnAttach() override;   // snapshots default values as start state
    void OnReset()  override;   // restores to last snapshotted start state

    // Call after editing the scene in editor mode so Return-to-Start resets
    // to the editor position rather than the JSON-loaded position.
    void SnapshotStartState();

    // ── Matrix helpers ────────────────────────────────────────────────────
    DirectX::XMMATRIX GetLocalMatrix() const;
    DirectX::XMMATRIX GetWorldMatrix() const;
    void SetFromLocalMatrix(DirectX::XMMATRIX m);

    // ── Serialization ─────────────────────────────────────────────────────
    std::string Serialize()                        const override;
    void        Deserialize(const std::string& json)    override;

private:
    DirectX::XMFLOAT3 m_startPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_startRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_startScale    = { 1.0f, 1.0f, 1.0f };
};

} // namespace engine
