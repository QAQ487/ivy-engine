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
#include <set>
#include <utility>
#include <type_traits>
#include "engine/physics/collider/collider.h"
#include "engine/core/debug_system.h"
#include "engine/debug_draw/debug_drawer.h"

namespace {
	using namespace DirectX;

	std::set<std::pair<GameObject*, GameObject*>> g_triggerPairsPrev;
	std::set<std::pair<GameObject*, GameObject*>> g_triggerPairsCurrent;
	std::set<std::pair<GameObject*, GameObject*>> g_contactPairsPrev;
	std::set<std::pair<GameObject*, GameObject*>> g_contactPairsCurrent;
	SceneBase* g_lastTriggerScene = nullptr;
}

// File-scope so public PhysicsSystem_* APIs can read/write (not anonymous).
static PhysicsDebugStats g_debugStats{};

struct ContactFlash {
	DirectX::XMFLOAT3 point{};
	DirectX::XMFLOAT3 tip{};
	int framesLeft = 0;
};
static std::vector<ContactFlash> g_contactFlashes;
constexpr int kContactFlashFrames = 18;
constexpr float kContactFlashRadius = 0.2f;

namespace {
	using namespace DirectX;

	void InsertTriggerPair(GameObject* a, GameObject* b) {
		if (!a || !b || a == b) return;
		if (b < a) std::swap(a, b);
		g_triggerPairsCurrent.insert({ a, b });
		++g_debugStats.triggers;
	}

	void DiffAndDispatchTriggerEvents() {
		// Enters first (current - prev), then exits (prev - current).
		for (const auto& pair : g_triggerPairsCurrent) {
			if (g_triggerPairsPrev.count(pair) == 0) {
				GameObject* a = pair.first;
				GameObject* b = pair.second;
				if (a) a->OnTriggerEnter(b);
				if (b) b->OnTriggerEnter(a);
			}
		}
		for (const auto& pair : g_triggerPairsPrev) {
			if (g_triggerPairsCurrent.count(pair) == 0) {
				GameObject* a = pair.first;
				GameObject* b = pair.second;
				if (a) a->OnTriggerExit(b);
				if (b) b->OnTriggerExit(a);
			}
		}
		g_triggerPairsPrev.swap(g_triggerPairsCurrent);
		g_triggerPairsCurrent.clear();
	}

	void CommitContactPairs() {
		g_contactPairsPrev.swap(g_contactPairsCurrent);
		g_contactPairsCurrent.clear();
	}

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

	// Static/static (or any non-swept) trigger overlap: CheckCollision + insert only.
	void DoTriggerOverlapCheck(GameObject* a, Collider* ca,
	                           GameObject* b, Collider* cb) {
		if (!a || !b || !ca || !cb) return;
		if (!BroadPhasePass(ca, cb)) return;
		if (ca->CheckCollision(cb)) {
			++g_debugStats.pairs;
			InsertTriggerPair(a, b);
		}
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
		const bool isTriggerPair = ca->IsTrigger() || cb->IsTrigger();

		// Thin mesh walls: discrete CheckCollision at sub-steps often misses when
		// a fast sphere jumps past zero-thickness triangles. Sphere↔Mesh
		// ResolveCollision already runs continuous raycast CCD along prev→cur.
		const bool sphereVsMesh =
			ca->GetType() == ColliderType::SPHERE && cb->GetType() == ColliderType::MESH;
		const bool meshVsSphere =
			ca->GetType() == ColliderType::MESH && cb->GetType() == ColliderType::SPHERE;
		if (sphereVsMesh || meshVsSphere) {
			a->SetPosition(curPos);
			if (isTriggerPair) {
				if (ca->CheckCollision(cb)) {
					++g_debugStats.pairs;
					InsertTriggerPair(a, b);
				}
				return;
			}
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
				++g_debugStats.pairs;
				if (isTriggerPair) {
					InsertTriggerPair(a, b);
					a->SetPosition(curPos);
					return;
				}

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

				// Floor-slide: mid-path ground overlap (gravity dip into the floor while
				// moving fast) aborts at t<1 and truncates horizontal travel so dash
				// displacement matches walk speed. Contact slop leaves CheckCollision
				// true even after a pure-Y resolve, so gate on vertical push instead:
				// if depenetration was floor-like, complete intended XZ then re-resolve.
				if (step < nSteps) {
					const XMFLOAT3 resolved = a->GetPosition();
					const float pushX = resolved.x - testPos.x;
					const float pushZ = resolved.z - testPos.z;
					const float pushY = resolved.y - testPos.y;
					const float horizPushSq = pushX * pushX + pushZ * pushZ;
					const bool floorLike = pushY > 1e-5f && horizPushSq < 1e-6f;
					if (floorLike) {
						a->SetPosition({ curPos.x, resolved.y, curPos.z });
						if (IsDynamic(a)) ca->ResolveCollision(cb);
						if (bDynamic)      cb->ResolveCollision(ca);
					}
				}
				return; // Stop at the first (earliest) collision along the path
			}
		}

		// No collision detected along the entire path – restore intended position.
		a->SetPosition(curPos);
	}
}

void PhysicsSystem_ClearTriggerPairs() {
	g_triggerPairsPrev.clear();
	g_triggerPairsCurrent.clear();
	g_contactPairsPrev.clear();
	g_contactPairsCurrent.clear();
	g_lastTriggerScene = nullptr;
}

void PhysicsSystem_NotifyObjectDestroyed(GameObject* obj) {
	if (!obj) return;

	std::vector<std::pair<GameObject*, GameObject*>> toNotify;
	auto collect = [&](std::set<std::pair<GameObject*, GameObject*>>& s) {
		for (const auto& pair : s) {
			if (pair.first == obj || pair.second == obj)
				toNotify.push_back(pair);
		}
	};
	collect(g_triggerPairsPrev);
	collect(g_triggerPairsCurrent);

	// Dedup (pair may be in both sets).
	std::sort(toNotify.begin(), toNotify.end());
	toNotify.erase(std::unique(toNotify.begin(), toNotify.end()), toNotify.end());

	for (const auto& pair : toNotify) {
		GameObject* a = pair.first;
		GameObject* b = pair.second;
		if (a) a->OnTriggerExit(b);
		if (b) b->OnTriggerExit(a);
		g_triggerPairsPrev.erase(pair);
		g_triggerPairsCurrent.erase(pair);
	}

	auto eraseContact = [&](std::set<std::pair<GameObject*, GameObject*>>& s) {
		for (auto it = s.begin(); it != s.end(); ) {
			if (it->first == obj || it->second == obj)
				it = s.erase(it);
			else
				++it;
		}
	};
	eraseContact(g_contactPairsPrev);
	eraseContact(g_contactPairsCurrent);
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

namespace {
	// Per-object cached data for the sweep-and-prune X-axis pre-filter below.
	// minX/maxX cover the object's full swept extent this frame (current and,
	// for dynamic objects, previous collider center) plus its approximate
	// bounding radius, doubled as an extra safety margin against mid-pass
	// position corrections nudging it slightly outside the precomputed range.
	struct BroadPhaseEntry {
		GameObject* obj;
		Collider*   collider;
		bool        isDynamic;
		XMFLOAT3    prevCenter; // only meaningful when isDynamic
		float       minX, maxX;
	};
}

// 衝突判定・解決パス（速度積分なし）。
// m_previousPosition から現在位置までのスウェプトブロードフェーズを使用し、
// 大きな変位に対してはサブステップナローフェーズで解決する。
void PhysicsSystem_ResolveCollisions() {
	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;

	g_debugStats = {};

	if (scene != g_lastTriggerScene) {
		PhysicsSystem_ClearTriggerPairs();
		g_lastTriggerScene = scene;
	}

	const auto& objs = scene->GetObjects();
	if (objs.empty()) return;

	// Build a single sweep-and-prune list covering every collidable object
	// (dynamic and static). Pairs are only ever accepted below if their
	// X-extents overlap here, but since BroadPhasePassSwept independently
	// requires overlap on X too, this list can only ever produce a superset
	// of the pairs the exact broad phase would accept anyway; it only changes
	// how many candidate pairs get tested (perf), never collision outcomes.
	std::vector<BroadPhaseEntry> entries;
	entries.reserve(objs.size());
	bool anyDynamic = false;
	bool anyTrigger = false;
	for (auto* o : objs) {
		if (!o || !o->IsActive()) continue;
		Collider* c = o->GetCollider();
		if (!c) continue;

		BroadPhaseEntry e;
		e.obj = o;
		e.collider = c;
		e.isDynamic = IsDynamic(o);
		anyDynamic = anyDynamic || e.isDynamic;
		anyTrigger = anyTrigger || c->IsTrigger();

		const float radius = GetApproxRadius(c);
		const XMFLOAT3 curCenter = c->GetWorldCenter();
		float minX = curCenter.x - radius;
		float maxX = curCenter.x + radius;

		if (e.isDynamic) {
			// Same previous-collider-center approximation as BroadPhasePassSwept
			// uses today: previous origin plus this frame's local-to-world
			// center offset (rotation assumed roughly unchanged since last frame).
			const XMFLOAT3 curPos = o->GetPosition();
			e.prevCenter = {
				o->m_previousPosition.x + (curCenter.x - curPos.x),
				o->m_previousPosition.y + (curCenter.y - curPos.y),
				o->m_previousPosition.z + (curCenter.z - curPos.z)
			};
			minX = (std::min)(minX, e.prevCenter.x - radius);
			maxX = (std::max)(maxX, e.prevCenter.x + radius);
		} else {
			e.prevCenter = curCenter;
		}

		// Extra safety margin (one more radius) to hedge against a's position
		// being nudged by an earlier resolution this same pass.
		e.minX = minX - radius;
		e.maxX = maxX + radius;
		entries.push_back(e);
	}
	g_debugStats.colliders = static_cast<int>(entries.size());
	if (entries.size() < 2 || (!anyDynamic && !anyTrigger)) {
		g_contactPairsCurrent.clear();
		CommitContactPairs();
		return;
	}

	g_triggerPairsCurrent.clear();
	g_contactPairsCurrent.clear();

	std::sort(entries.begin(), entries.end(),
		[](const BroadPhaseEntry& l, const BroadPhaseEntry& r) { return l.minX < r.minX; });

	for (size_t i = 0; i < entries.size(); ++i) {
		for (size_t j = i + 1; j < entries.size() && entries[j].minX <= entries[i].maxX; ++j) {
			const BroadPhaseEntry& e1 = entries[i];
			const BroadPhaseEntry& e2 = entries[j];

			const bool eitherTrigger =
				e1.collider->IsTrigger() || e2.collider->IsTrigger();

			// Static–static: only process when a trigger is involved (cheap overlap).
			if (!e1.isDynamic && !e2.isDynamic) {
				if (!eitherTrigger) continue;
				if (!e1.obj->IsActive() || !e2.obj->IsActive()) continue;
				if (!e1.obj->CanCollideWith(e2.obj) || !e2.obj->CanCollideWith(e1.obj))
					continue;
				DoTriggerOverlapCheck(e1.obj, e1.collider, e2.obj, e2.collider);
				continue;
			}

			// Re-derive the exact same (a, b, bDynamic) roles the original
			// nested loop used: a is always the dynamic side; when both are
			// dynamic, the lower-address object is "a" so each dynamic/dynamic
			// pair still only gets resolved once (mirrors the old `b < a` skip).
			GameObject* a; GameObject* b;
			Collider* ca; Collider* cb;
			XMFLOAT3 prevCenterA;
			bool bDynamic;
			if (e1.isDynamic && e2.isDynamic) {
				const bool e1First = e1.obj < e2.obj;
				const BroadPhaseEntry& ea = e1First ? e1 : e2;
				const BroadPhaseEntry& eb = e1First ? e2 : e1;
				a = ea.obj; ca = ea.collider; prevCenterA = ea.prevCenter;
				b = eb.obj; cb = eb.collider;
				bDynamic = true;
			} else if (e1.isDynamic) {
				a = e1.obj; ca = e1.collider; prevCenterA = e1.prevCenter;
				b = e2.obj; cb = e2.collider;
				bDynamic = false;
			} else {
				a = e2.obj; ca = e2.collider; prevCenterA = e2.prevCenter;
				b = e1.obj; cb = e1.collider;
				bDynamic = false;
			}

			if (!a->IsActive() || !b->IsActive()) continue;

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

	DiffAndDispatchTriggerEvents();
	CommitContactPairs();
}

bool PhysicsSystem_Raycast(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance, RayHit& outHit,
	bool skipTriggers) {
	outHit = {};
	if (maxDistance <= 0.0f) return false;

	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return false;

	using namespace DirectX;
	XMVECTOR vDir = XMLoadFloat3(&dir);
	float dirLen = XMVectorGetX(XMVector3Length(vDir));
	if (dirLen < 1e-8f) return false;
	XMVECTOR vDirN = XMVectorScale(vDir, 1.0f / dirLen);
	XMFLOAT3 dirN;
	XMStoreFloat3(&dirN, vDirN);

	float bestT = maxDistance;
	bool hit = false;
	RayHit best{};

	for (auto* obj : scene->GetObjects()) {
		if (!obj || !obj->IsActive()) continue;
		Collider* c = obj->GetCollider();
		if (!c) continue;
		if (skipTriggers && c->IsTrigger()) continue;

		const XMFLOAT3 center = c->GetWorldCenter();
		const float radius = GetApproxRadius(c);
		// Closest point on ray to sphere center; cull if beyond radius + maxDistance.
		XMVECTOR vOrig = XMLoadFloat3(&origin);
		XMVECTOR vC = XMLoadFloat3(&center);
		XMVECTOR toC = XMVectorSubtract(vC, vOrig);
		float tClosest = XMVectorGetX(XMVector3Dot(toC, vDirN));
		if (tClosest + radius < 0.0f) continue;
		if (tClosest - radius > bestT) continue;
		XMVECTOR closest = XMVectorAdd(vOrig, XMVectorScale(vDirN, tClosest));
		XMVECTOR diff = XMVectorSubtract(closest, vC);
		float dist2 = XMVectorGetX(XMVector3LengthSq(diff));
		if (dist2 > (radius + 0.05f) * (radius + 0.05f) &&
			(tClosest < 0.0f || tClosest > bestT)) {
			// Still allow RayIntersect if sphere could clip the segment — use
			// conservative: only skip when clearly outside enlarged sphere of segment.
			float segR = radius + 0.05f;
			if (dist2 > segR * segR) continue;
		}

		float t = 0.0f;
		XMFLOAT3 normal{};
		if (!c->RayIntersect(origin, dirN, bestT, t, normal))
			continue;
		if (t <= 0.0f || t > bestT) continue;

		bestT = t;
		hit = true;
		best.object = obj;
		best.distance = t;
		best.normal = normal;
		best.point = {
			origin.x + dirN.x * t,
			origin.y + dirN.y * t,
			origin.z + dirN.z * t
		};
	}

	if (hit)
		outHit = best;
	return hit;
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

PhysicsDebugStats PhysicsSystem_GetDebugStats() {
	return g_debugStats;
}

void PhysicsSystem_NotifyContactResolved() {
	++g_debugStats.contacts;
}

bool PhysicsSystem_WasContactPairKnown(GameObject* a, GameObject* b) {
	if (!a || !b || a == b) return false;
	if (b < a) std::swap(a, b);
	const std::pair<GameObject*, GameObject*> pair{ a, b };
	const bool known = g_contactPairsPrev.count(pair) > 0;
	g_contactPairsCurrent.insert(pair);
	return known;
}

void PhysicsSystem_QueueContactFlash(const DirectX::XMFLOAT3& point,
	const DirectX::XMFLOAT3& normalTip) {
	if (!DebugSystem::CollisionVizEnabled()) return;
	g_contactFlashes.push_back({ point, normalTip, kContactFlashFrames });
}

void PhysicsSystem_PrepareContactFlashes() {
	if (!DebugSystem::CollisionVizEnabled()) {
		g_contactFlashes.clear();
		return;
	}
	constexpr DirectX::XMFLOAT4 kVizContact = { 1.0f, 0.0f, 0.0f, 1.0f };
	constexpr DirectX::XMFLOAT3 kZero = { 0.0f, 0.0f, 0.0f };
	for (auto it = g_contactFlashes.begin(); it != g_contactFlashes.end(); ) {
		DebugDrawer::QueueSphere(it->point, kZero, kContactFlashRadius, kZero, kVizContact);
		DebugDrawer::QueueLine(it->point, it->tip, kVizContact);
		if (--it->framesLeft <= 0)
			it = g_contactFlashes.erase(it);
		else
			++it;
	}
}

void PhysicsSystem_ClearDebugDrawQueue() {
	g_contactFlashes.clear();
}

void PhysicsSystem_FlushDebugDraw() {
	// F11 draws every collider each frame so wires stay visible while airborne
	// and solids stay green (triggers yellow — never paint solid mesh partners).
	if (!DebugSystem::CollisionVizEnabled()) return;

	SceneBase* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;

	constexpr DirectX::XMFLOAT4 kVizSolid = { 0.0f, 1.0f, 0.0f, 1.0f };
	constexpr DirectX::XMFLOAT4 kVizTrig = { 1.0f, 1.0f, 0.0f, 1.0f };
	for (GameObject* obj : scene->GetObjects()) {
		if (!obj || !obj->IsActive()) continue;
		Collider* col = obj->GetCollider();
		if (!col) continue;
		col->DebugDraw(col->IsTrigger() ? kVizTrig : kVizSolid);
	}
}

