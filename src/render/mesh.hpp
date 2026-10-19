#pragma once
// mesh.hpp — geometria procedurale. Nessun file esterno, nessuna texture.

#include "core/food.hpp"

#include <cstdint>
#include <vector>

namespace w3 {

struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

MeshData makeSphere(int segments, int rings);
MeshData makeCube();
MeshData makeStar();
MeshData makeOctahedron();
MeshData makeTorus(int majorSegments, int minorSegments);

// Mesh associata a una forma del catalogo cibo (indice = MeshShape).
MeshData makeMesh(MeshShape shape);

}  // namespace w3
