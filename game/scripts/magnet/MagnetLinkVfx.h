#pragma once

#include "MagnetHelixPath.h"
#include "engine/scene/scene_node.h"

#include <DirectXMath.h>
#include <cstdint>

class MagnetLinkVfx {
public:
	static MagnetLinkVfx& Instance();

	void Tick(float dt);
	void Reset();
	void Show(const engine::GameObject& source, const engine::GameObject& target, int type);
	void Show(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, int type);
	void Hide();

	void SetSettings(const MagnetHelixPath::MagnetHelixSettings& settings) { m_settings = settings; }
	const MagnetHelixPath::MagnetHelixSettings& GetSettings() const { return m_settings; }

	bool IsActive() const { return m_showMagnetLink; }

private:
	MagnetHelixPath::MagnetHelixSettings m_settings;
	std::uint32_t m_linkEmitterId = 0;
	float m_magnetLinkPhase = 0.0f;
	int m_magnetLinkType = 0; // 1=pull, 2=push, 3=orbit
	bool m_showMagnetLink = false;
	float m_helixRefreshTimer = 0.0f;
	DirectX::XMFLOAT3 m_magnetLinkStart = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_magnetLinkEnd = { 0.0f, 0.0f, 0.0f };
	const engine::GameObject* m_magnetLinkSource = nullptr;
	const engine::GameObject* m_magnetLinkTarget = nullptr;
	MagnetHelixPath::LinkFrame m_prevLinkFrame;
	float m_prevPhase = 0.0f;
	bool m_hasPrevFrame = false;
};
