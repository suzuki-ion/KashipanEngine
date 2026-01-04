#include "Collider.h"

#include "Objects/Collision/CollisionAlgorithms2D.h"
#include "Objects/Collision/CollisionAlgorithms3D.h"

#include <algorithm>

namespace KashipanEngine {

namespace {
inline bool ShouldTest(
    const std::bitset<ColliderInfo2D::kMaxAttributes> &selfAttr,
    const std::bitset<ColliderInfo2D::kMaxAttributes> &selfIgnore,
    const std::bitset<ColliderInfo2D::kMaxAttributes> &otherAttr) {
    (void)selfAttr;
    return (selfIgnore & otherAttr).none();
}

inline bool Intersects2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs);
            } else {
                return false;
            }
        },
        a, b);
}

inline bool Intersects3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs);
            } else {
                return false;
            }
        },
        a, b);
}

inline HitInfo ComputeHit2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) -> HitInfo {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::ComputeHit(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms2D::ComputeHit(lhs, rhs);
            } else if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs); }) {
                HitInfo hi;
                hi.isHit = KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs);
                return hi;
            } else {
                return HitInfo{};
            }
        },
        a, b);
}

inline HitInfo ComputeHit3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) -> HitInfo {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::ComputeHit(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms3D::ComputeHit(lhs, rhs);
            } else if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs); }) {
                HitInfo hi;
                hi.isHit = KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs);
                return hi;
            } else {
                return HitInfo{};
            }
        },
        a, b);
}

} // namespace

Collider::ColliderID Collider::Add(const ColliderInfo2D &info) {
    const ColliderID id = nextId_++;
    colliders2D_.push_back({id, info});
    return id;
}

Collider::ColliderID Collider::Add(const ColliderInfo3D &info) {
    const ColliderID id = nextId_++;
    colliders3D_.push_back({id, info});
    return id;
}

bool Collider::Remove2D(ColliderID id) {
    return EraseById(colliders2D_, id);
}

bool Collider::Remove3D(ColliderID id) {
    return EraseById(colliders3D_, id);
}

void Collider::Clear2D() {
    colliders2D_.clear();
    prevPairs2D_.clear();
}

void Collider::Clear3D() {
    colliders3D_.clear();
    prevPairs3D_.clear();
}

std::vector<Collider::HitPair2D> Collider::CheckAll2D() const {
    std::vector<HitPair2D> hits;
    const size_t n = colliders2D_.size();
    for (size_t i = 0; i < n; ++i) {
        const auto &ai = colliders2D_[i];
        if (!ai.info.enabled) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const auto &bi = colliders2D_[j];
            if (!bi.info.enabled) continue;

            if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
                !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
                continue;
            }

            if (Intersects2D(ai.info.shape, bi.info.shape)) {
                hits.push_back({ai.id, bi.id});
            }
        }
    }
    return hits;
}

std::vector<Collider::HitPair3D> Collider::CheckAll3D() const {
    std::vector<HitPair3D> hits;
    const size_t n = colliders3D_.size();
    for (size_t i = 0; i < n; ++i) {
        const auto &ai = colliders3D_[i];
        if (!ai.info.enabled) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const auto &bi = colliders3D_[j];
            if (!bi.info.enabled) continue;

            if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
                !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
                continue;
            }

            if (Intersects3D(ai.info.shape, bi.info.shape)) {
                hits.push_back({ai.id, bi.id});
            }
        }
    }
    return hits;
}

bool Collider::Check2D(ColliderID a, ColliderID b) const {
    const auto *pa = Find2D(a);
    const auto *pb = Find2D(b);
    if (!pa || !pb) return false;
    if (!pa->info.enabled || !pb->info.enabled) return false;

    if (!ShouldTest(pa->info.attribute, pa->info.ignoreAttribute, pb->info.attribute) ||
        !ShouldTest(pb->info.attribute, pb->info.ignoreAttribute, pa->info.attribute)) {
        return false;
    }

    return Intersects2D(pa->info.shape, pb->info.shape);
}

bool Collider::Check3D(ColliderID a, ColliderID b) const {
    const auto *pa = Find3D(a);
    const auto *pb = Find3D(b);
    if (!pa || !pb) return false;
    if (!pa->info.enabled || !pb->info.enabled) return false;

    if (!ShouldTest(pa->info.attribute, pa->info.ignoreAttribute, pb->info.attribute) ||
        !ShouldTest(pb->info.attribute, pb->info.ignoreAttribute, pa->info.attribute)) {
        return false;
    }

    return Intersects3D(pa->info.shape, pb->info.shape);
}

std::uint64_t Collider::MakePairKey(ColliderID a, ColliderID b) {
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(a) << 32) | static_cast<std::uint64_t>(b);
}

void Collider::Dispatch2D(ColliderID a, ColliderID b, const HitInfo &hitInfo, bool wasHit) {
    const auto *ea = Find2D(a);
    const auto *eb = Find2D(b);
    if (!ea || !eb) return;

    const bool isHitNow = hitInfo.isHit;

    if (isHitNow) {
        if (!wasHit) {
            if (ea->info.onCollisionEnter) ea->info.onCollisionEnter(hitInfo);
            if (eb->info.onCollisionEnter) eb->info.onCollisionEnter(hitInfo);
        }
        if (ea->info.onCollisionStay) ea->info.onCollisionStay(hitInfo);
        if (eb->info.onCollisionStay) eb->info.onCollisionStay(hitInfo);
    } else {
        if (wasHit) {
            if (ea->info.onCollisionExit) ea->info.onCollisionExit(hitInfo);
            if (eb->info.onCollisionExit) eb->info.onCollisionExit(hitInfo);
        }
    }
}

void Collider::Dispatch3D(ColliderID a, ColliderID b, const HitInfo &hitInfo, bool wasHit) {
    const auto *ea = Find3D(a);
    const auto *eb = Find3D(b);
    if (!ea || !eb) return;

    const bool isHitNow = hitInfo.isHit;

    if (isHitNow) {
        if (!wasHit) {
            if (ea->info.onCollisionEnter) ea->info.onCollisionEnter(hitInfo);
            if (eb->info.onCollisionEnter) eb->info.onCollisionEnter(hitInfo);
        }
        if (ea->info.onCollisionStay) ea->info.onCollisionStay(hitInfo);
        if (eb->info.onCollisionStay) eb->info.onCollisionStay(hitInfo);
    } else {
        if (wasHit) {
            if (ea->info.onCollisionExit) ea->info.onCollisionExit(hitInfo);
            if (eb->info.onCollisionExit) eb->info.onCollisionExit(hitInfo);
        }
    }
}

void Collider::Update2D() {
    std::vector<std::uint64_t> cur;

    const size_t n = colliders2D_.size();
    cur.reserve((n * (n - 1)) / 2);

    for (size_t i = 0; i < n; ++i) {
        const auto &ai = colliders2D_[i];
        if (!ai.info.enabled) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const auto &bi = colliders2D_[j];
            if (!bi.info.enabled) continue;

            if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
                !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
                continue;
            }

            const HitInfo hi = ComputeHit2D(ai.info.shape, bi.info.shape);
            const std::uint64_t key = MakePairKey(ai.id, bi.id);

            const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
            Dispatch2D(ai.id, bi.id, hi, wasHit);

            if (hi.isHit) cur.push_back(key);
        }
    }

    std::sort(cur.begin(), cur.end());
    prevPairs2D_ = std::move(cur);
}

void Collider::Update3D() {
    std::vector<std::uint64_t> cur;

    const size_t n = colliders3D_.size();
    cur.reserve((n * (n - 1)) / 2);

    for (size_t i = 0; i < n; ++i) {
        const auto &ai = colliders3D_[i];
        if (!ai.info.enabled) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const auto &bi = colliders3D_[j];
            if (!bi.info.enabled) continue;

            if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
                !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
                continue;
            }

            const HitInfo hi = ComputeHit3D(ai.info.shape, bi.info.shape);
            const std::uint64_t key = MakePairKey(ai.id, bi.id);

            const bool wasHit = std::binary_search(prevPairs3D_.begin(), prevPairs3D_.end(), key);
            Dispatch3D(ai.id, bi.id, hi, wasHit);

            if (hi.isHit) cur.push_back(key);
        }
    }

    std::sort(cur.begin(), cur.end());
    prevPairs3D_ = std::move(cur);
}

const Collider::Entry<ColliderInfo2D> *Collider::Find2D(ColliderID id) const {
    for (const auto &e : colliders2D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const Collider::Entry<ColliderInfo3D> *Collider::Find3D(ColliderID id) const {
    for (const auto &e : colliders3D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

} // namespace KashipanEngine
