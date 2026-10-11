#pragma once

// Master SDK Header for Ignis Engine Games & Scripts
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <entt/entt.hpp>
#if __has_include(<imgui/imgui.h>)
#include <imgui/imgui.h>
#elif __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include "api.h"
#include "input.h"
#include "audio.h"

#include "scene/entity.h"
#include "scene/components.h"
#include "scene/scriptable_entity.h"
#include "scene/scene.h"

#include "physics.h"

namespace Ignis::Physics {

inline Entity RaycastHit::GetHitEntity(Scene* scene) const
{
    return Entity{ static_cast<entt::entity>(EntityHandle), scene };
}

inline bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit, uint32_t layerMask, bool includeTriggers)
{
    if (!GetContext() || !GetContext()->Physics.Raycast)
        return false;

    RawRaycastHit rawHit{};
    bool res = GetContext()->Physics.Raycast(&scene, ray.Origin, ray.Direction, maxDistance, &rawHit, layerMask, includeTriggers);
    if (res)
    {
        outHit.Hit = rawHit.Hit;
        outHit.Distance = rawHit.Distance;
        outHit.Point = rawHit.Point;
        outHit.Normal = rawHit.Normal;
        outHit.EntityHandle = rawHit.EntityHandle;
        outHit.HitEntity = Entity{ static_cast<entt::entity>(rawHit.EntityHandle), &scene };
    }
    return res;
}

inline bool RaycastAll(Scene& scene, const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits, uint32_t layerMask, bool includeTriggers)
{
    outHits.clear();
    if (!GetContext() || !GetContext()->Physics.RaycastAll)
        return false;

    RawRaycastHit rawBuffer[64];
    int count = GetContext()->Physics.RaycastAll(&scene, ray.Origin, ray.Direction, maxDistance, rawBuffer, 64, layerMask, includeTriggers);
    for (int i = 0; i < count; i++)
    {
        RaycastHit hit;
        hit.Hit = rawBuffer[i].Hit;
        hit.Distance = rawBuffer[i].Distance;
        hit.Point = rawBuffer[i].Point;
        hit.Normal = rawBuffer[i].Normal;
        hit.EntityHandle = rawBuffer[i].EntityHandle;
        hit.HitEntity = Entity{ static_cast<entt::entity>(rawBuffer[i].EntityHandle), &scene };
        outHits.push_back(hit);
    }
    return !outHits.empty();
}

} // namespace Ignis::Physics
