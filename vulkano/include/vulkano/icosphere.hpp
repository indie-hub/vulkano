#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <vulkano/mesh.hpp>

namespace vulkano {

// Generate an icosphere mesh with given subdivisions [0..6].
// Radius is 1.0; positions lie on the unit sphere.
Mesh make_icosphere(int subdivisions);

} // namespace vulkano

