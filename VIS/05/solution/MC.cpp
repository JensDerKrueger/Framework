#include "MC.h"
#include "MC.inl"
#include <algorithm> // std::clamp

Isosurface::Isosurface(const Volume& volume, uint8_t isovalue) {
  std::array<Vertex, 12> verticesOnEdges;

  for (size_t l = 0; l < volume.depth-1; ++l) {
    for(size_t v = 0; v < volume.height-1; ++v) {
      for(size_t u = 0; u < volume.width-1; ++u) {
      
        // fetch data from the volume and
        // classify vertices and compute case index
        std::array<uint8_t, 8> cell{};
        std::array<Vec3, 8> normals{};
        uint8_t mcCase{0};
        for (uint8_t i = 0;i<8;++i) {
          const size_t index = (u+size_t(vertexPosTable[i][0])) +
                               (v+size_t(vertexPosTable[i][1])) * volume.width +
                               (l+size_t(vertexPosTable[i][2])) * volume.width*volume.height;
          cell[i]    = volume.data[index];
          normals[i] = volume.normals[index];
          mcCase += uint8_t(cell[i] > isovalue) << i;
        }
        
        // bail out if cell is empty
        if (!edgeTable[mcCase]) continue;

        // compute offset to current cube
        const Vec3 cubeOffset{
          (float(u)-float(volume.width)/2)/float(volume.maxSize),
          (float(v)-float(volume.height)/2)/float(volume.maxSize),
          (float(l)-float(volume.depth)/2)/float(volume.maxSize)
        };
        
        // interpolate vertices and normals
        for (uint8_t i = 0;i<12;++i) {
          if(edgeTable[mcCase] & 1<<i) {
            const auto [i0,i1] = edgeToVertexTable[i];
            const float d0 = float(cell[i0]);
            const float d1 = float(cell[i1]);
            const float alpha = std::clamp((d0-isovalue)/(d0-d1),0.0f,1.0f);
            const Vec3& v0 = vertexPosTable[i0];
            const Vec3& v1 = vertexPosTable[i1];
            const Vec3 positionInCube{v0 + (v1-v0)*alpha};
            const Vec3 normal{normals[i0] + (normals[i1]-normals[i0]) * alpha};
            verticesOnEdges[i] = Vertex{Vec3{volume.scale * (cubeOffset + positionInCube/float(volume.maxSize))}, Vec3::normalize(normal)};
          }
        }

        // add triangles to isosurface
        for (const uint8_t index : trisTable[mcCase]) {
          vertices.push_back(verticesOnEdges[index]);
        }
      }
    }
  }
}
