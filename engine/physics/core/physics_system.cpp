/**===========================================================================

 File   :  [physics_system.cpp]

 Author : Hayato Otsuki
 Date   : 2025/12/2
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "physics_system.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include <DirectXMath.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include "engine/physics/collider/collider.h"

namespace {
	using namespace DirectX;

	// 動的剛体かどうかを判定（キネマティックでなく、質量がある）
	inline bool IsDynamic(const GameObject* obj) {
		const Rigidbody& rb = obj->GetRigidbody();
		return !rb.IsKinematic() && rb.GetMass() > 0.0f;
	}

	// 各コライダーの概算バウンディングスフィア半径（ブロードフェーズ用の近似値）
	inline float GetApproxRadius(const Collider* c) {
		if (!c) return 0.0f;
		switch (c->GetType()) {
			case ColliderType::SPHERE:
			{
				auto* s = static_cast<const SphereCollider*>(c);
				return s->GetRadius();
			}
			case ColliderType::BOX:
			{
				auto* b = static_cast<const BoxCollider*>(c);
				const auto sz = b->GetSize();
				// 対角線の半分を半径として近似
				return 0.5f * std::sqrt(sz.x * sz.x + sz.y * sz.y + sz.z * sz.z);
			}
			case ColliderType::CAPSULE:
			{
				auto* cp = static_cast<const CapsuleCollider*>(c);
				// カプセルは「半径 + 高さ/2」を目安の半径に（少し大きめ）
				return cp->GetRadius() + 0.5f * cp->GetHeight();
			}
			case ColliderType::MESH:
			{
				auto* m = static_cast<const MeshCollider*>(c);
				return m->GetWorldBoundingSphereRadius();
			}
			case ColliderType::CYLINDER:
			{
				auto* cy = static_cast<const CylinderCollider*>(c);
				const float hh = 0.5f * cy->GetHeight();
				return std::sqrt(cy->GetRadius() * cy->GetRadius() + hh * hh);
			}
			default:
				return 0.0f;
		}
	}

	// ブロードフェーズ（概算距離判定）
	inline bool BroadPhasePass(const Collider* ca, const Collider* cb) {
		if (!ca || !cb) return false;

		const auto aC = ca->GetWorldCenter();
		const auto bC = cb->GetWorldCenter();
		const float dx = aC.x - bC.x;
		const float dy = aC.y - bC.y;
		const float dz = aC.z - bC.z;
		const float dist2 = dx * dx + dy * dy + dz * dz;

		const float ra = GetApproxRadius(ca);
		const float rb = GetApproxRadius(cb);
		const float r = ra + rb + 0.05f; // 余裕マージンを少し追加
		return dist2 <= (r * r);
	}

	// スウェプトブロードフェーズ: コライダー A が prevCenterA から現在位置まで移動した
	// 際にコライダー B と交差する可能性があるかをカプセル vs スフィアで判定する。
	// これにより高速移動（軌道回転など）によるトンネリングを早期検出できる。
	inline bool BroadPhasePassSwept(const Collider* ca, const XMFLOAT3& prevCenterA,
	                                 const Collider* cb) {
		if (!ca || !cb) return false;

		const XMFLOAT3 curCenterA = ca->GetWorldCenter();
		const XMFLOAT3 bC         = cb->GetWorldCenter();
		const float    ra         = GetApproxRadius(ca);
		const float    rb         = GetApproxRadius(cb);

		// Segment from prevCenterA to curCenterA; find closest point on segment to bC.
		float sdx  = curCenterA.x - prevCenterA.x;
		float sdy  = curCenterA.y - prevCenterA.y;
		float sdz  = curCenterA.z - prevCenterA.z;
		float len2 = sdx * sdx + sdy * sdy + sdz * sdz;

		float t = 0.0f;
		if (len2 > 1e-8f) {
			float px = bC.x - prevCenterA.x;
			float py = bC.y - prevCenterA.y;
			float pz = bC.z - prevCenterA.z;
			t = (px * sdx + py * sdy + pz * sdz) / len2;
			t = max(0.0f, min(1.0f, t));
		}

		float closestX = prevCenterA.x + t * sdx;
		float closestY = prevCenterA.y + t * sdy;
		float closestZ = prevCenterA.z + t * sdz;

		float cdx    = bC.x - closestX;
		float cdy    = bC.y - closestY;
		float cdz    = bC.z - closestZ;
		float cdist2 = cdx * cdx + cdy * cdy + cdz * cdz;

		const float r = ra + rb + 0.05f;
		return cdist2 <= (r * r);
	}

	// スウェプトナローフェーズ: A が prevPos から現在位置まで移動した経路を
	// サブステップに分割して衝突判定する。衝突が検出された最初のサブステップ位置
	// (testPos) で ResolveCollision を呼び出し、位置補正と速度のキャンセルを行う。
	// 高速軌道回転でのトンネリング防止に使用する。
	void DoSweptNarrowPhase(GameObject* a, Collider* ca,
	                        GameObject* b, Collider* cb,
	                        bool bDynamic) {
		const XMFLOAT3 prevPos = a->m_previousPosition;
		const XMFLOAT3 curPos  = a->GetPosition();

		// Thin mesh walls: discrete CheckCollision at sub-steps often misses when
		// a fast sphere jumps past zero-thickness triangles. Sphere↔Mesh
		// ResolveCollision already runs continuous raycast CCD along prev→cur.
		const bool sphereVsMesh =
			ca->GetType() == ColliderType::SPHERE && cb->GetType() == ColliderType::MESH;
		const bool meshVsSphere =
			ca->GetType() == ColliderType::MESH && cb->GetType() == ColliderType::SPHERE;
		if (sphereVsMesh || meshVsSphere) {
			a->SetPosition(curPos);
			if (IsDynamic(a))
				ca->ResolveCollision(cb);
			if (bDynamic)
				cb->ResolveCollision(ca);
			return;
		}

		float dx   = curPos.x - prevPos.x;
		float dy   = curPos.y - prevPos.y;
		float dz   = curPos.z - prevPos.z;
		float dLen = std::sqrt(dx * dx + dy * dy + dz * dz);

		// Number of sub-steps: at least 1, more when the displacement is large
		// relative to the collider radius (prevents tunneling through thin objects).
		// Step size ~0.25R so thin boxes still get sampled; cap raised for fast throws.
		float colliderR = GetApproxRadius(ca);
		int   nSteps    = 1;
		const float stepSize = colliderR * 0.25f;
		if (stepSize > 1e-4f && dLen > stepSize) {
			nSteps = static_cast<int>(std::ceil(dLen / stepSize));
			nSteps = min(nSteps, 64);
		}

		for (int step = 1; step <= nSteps; step++) {
			float    t       = static_cast<float>(step) / static_cast<float>(nSteps);
			XMFLOAT3 testPos = {
				prevPos.x + dx * t,
				prevPos.y + dy * t,
				prevPos.z + dz * t
			};
			a->SetPosition(testPos);

			// Re-check broad phase at this sub-step position (cheap early out).
			if (!BroadPhasePass(ca, cb)) continue;

			if (ca->CheckCollision(cb)) {
				// Collision found at testPos (A is penetrating B here).
				// Resolve from this position so that ResolveCollision can detect
				// the actual penetration depth, push A out, and zero the velocity
				// component directed into the surface.  Backing A up to the
				// pre-collision step (safePos) would leave zero penetration, causing
				// ResolveContact to skip both the position correction and the
				// velocity impulse – which is the root cause of the player falling
				// through the floor.
				if (IsDynamic(a)) ca->ResolveCollision(cb);
				if (bDynamic)      cb->ResolveCollision(ca);
				return; // Stop at the first (earliest) collision along the path
			}
		}

		// No collision detected along the entire path – restore intended position.
		a->SetPosition(curPos);
	}
}

// ポジションスナップショット: 動的オブジェクトの現在位置を m_previousPosition に保存する。
// スクリプト（軌道回転など）を実行する直前に呼び出すことで、スクリプト後の
// スウェプト衝突パスが正確な変位を把握できるようにする。
void PhysicsSystem_SavePositions() {
	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;
	for (auto* obj : scene->GetObjects()) {
		if (!obj || !obj->IsActive() || !obj->GetCollider()) continue;
		if (IsDynamic(obj)) {
			obj->m_previousPosition = obj->m_position;
		}
	}
}

// 衝突判定・解決パス（速度積分なし）。
// m_previousPosition から現在位置までのスウェプトブロードフェーズを使用し、
// 大きな変位に対してはサブステップナローフェーズで解決する。
void PhysicsSystem_ResolveCollisions() {
	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;

	const auto& objs = scene->GetObjects();
	if (objs.empty()) return;

	// ペア生成（動的ボディ vs 全オブジェクト）
	std::vector<GameObject*> dynamics;
	dynamics.reserve(objs.size());
	for (auto* o : objs) {
		if (!o || !o->IsActive() || !o->GetCollider()) continue;
		if (IsDynamic(o)) dynamics.push_back(o);
	}

	// 衝突判定と解決（スウェプトAABB）
	for (auto* a : dynamics) {
		if (!a || !a->IsActive()) continue;
		Collider* ca = a->GetCollider();
		if (!ca) continue;

		const XMFLOAT3 prevCenterA = a->m_previousPosition;

		for (auto* b : objs) {
			if (!b || b == a || !b->IsActive()) continue;
			Collider* cb = b->GetCollider();
			if (!cb) continue;

			const bool bDynamic = IsDynamic(b);
			// 動的同士の組み合わせは一度だけ処理
			if (bDynamic && b < a) continue;

			// ペアの衝突可否（チーム/タグ/レイヤーのルール）を確認
			if (!a->CanCollideWith(b) || !b->CanCollideWith(a)) continue;

			// スウェプトブロードフェーズ: 現在位置でのスフィアチェックに加え、
			// 前フレームから現フレームへの経路全体をカプセルで包んで判定する。
			const bool bpOk = BroadPhasePassSwept(ca, prevCenterA, cb);
			if (!bpOk) continue;

			// ナローフェーズ（スウェプト）: 変位が大きい場合はサブステップで判定する。
			DoSweptNarrowPhase(a, ca, b, cb, bDynamic);
		}
	}
}

// 物理システム更新
void PhysicsSystem_Update(double elapsed_time) {
	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;

	const auto& objs = scene->GetObjects();
	if (objs.empty()) return;

	// 1. 前フレーム位置の保存（スウェプト衝突検出用）
	for (auto* obj : objs) {
		if (!obj || !obj->IsActive() || !obj->GetCollider()) continue;
		if (IsDynamic(obj)) {
			obj->m_previousPosition = obj->m_position;
		}
	}

	// 2. 速度積分（動的ボディのみ）
	for (auto* obj : objs) {
		if (!obj || !obj->IsActive() || !obj->GetCollider()) continue;
		auto& rb = obj->GetRigidbody();
		if (IsDynamic(obj)) {
			DirectX::XMFLOAT3 pos = obj->GetPosition();
			rb.Integrate(pos, elapsed_time);
			obj->SetPosition(pos);
		}
	}

	// 3. 衝突判定と解決（スウェプトAABBを使用）
	PhysicsSystem_ResolveCollisions();
}