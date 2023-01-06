#pragma once

#include "systems/scene.h"

namespace cyb::renderer
{
    void ImportModel_3DS(const std::string& filename, scene::Scene& scene);
    ecs::Entity ImportModel_GLTF(const std::string& filename, scene::Scene& scene);
}