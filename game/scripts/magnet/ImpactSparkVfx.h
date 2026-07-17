#pragma once

#include <DirectXMath.h>
#include <cstdint>

class ImpactSparkVfx {
public:
	static ImpactSparkVfx& Instance();

	// Spawns a one-shot spark burst at position. normalXZ should be unit direction
	// in the XZ plane (from one body toward the other); can be zero.
	void Spawn(const DirectX::XMFLOAT3& position,
	           const DirectX::XMFLOAT3& normalXZ,
	           bool charged);

	void Reset();

private:
	ImpactSparkVfx() = default;

	void EnsureEmitter();

	std::uint32_t m_emitterId = 0;
};
