#include "MS.h"
#include "MS.inl"

Isoline::Isoline(const Image& image, uint8_t isovalue,
                 bool useAsymptoticDecider) {

  for(size_t v = 0; v < image.height-1; v++) {
    for(size_t u = 0; u < image.width-1; u++) {
      // fetch data from the image
      std::array<uint8_t, 4> cell{};
      for (uint8_t i = 0;i<4;++i) {
        const size_t index = ((u+size_t(vertexPosTable[i][0])) +
                             (v+size_t(vertexPosTable[i][1])) * image.width)
                             *image.componentCount;
        cell[i] = image.data[index];
      }
      
      // classify vertices and compute case index
      uint8_t msCase{0};
      for (uint8_t i = 0;i<4;++i)
        msCase += uint8_t(cell[i] > isovalue) << i;

      // bail out if cell is empty
      if (!edgeTable[msCase]) continue;
      
      // compute offset to current cell in normalized coordinates
      const Vec2 cellOffset{
        float(u)+0.5f,
        float(v)+0.5f
      };

      // interpolate vertices
      const Vec2 scale = Vec2{2.0f/float(image.width), 2.0f/float(image.height)};
      const Vec2 bias  = Vec2{-1,-1};
      for (uint8_t i = 0;i<4;++i) {
        if(edgeTable[msCase] & 1<<i) {
          const auto [i0,i1] = edgeToVertexTable[i];
          const float d0 = float(cell[i0]);
          const float d1 = float(cell[i1]);
          const float alpha = std::clamp((d0-isovalue)/(d0-d1),0.0f,1.0f);
          const Vec2& v0 = vertexPosTable[i0];
          const Vec2& v1 = vertexPosTable[i1];
          const Vec2 positionInCell{v0 + (v1-v0)*alpha};

          vertices.push_back((cellOffset + positionInCell)*scale + bias);
        }
      }
      
      // handle ambiguous cases
      if (useAsymptoticDecider && (msCase == 5 || msCase == 10)) {
        const float decider{float(cell[3]*cell[1]-cell[2]*cell[0]) /
                            float(cell[1]+cell[3]-cell[2]-cell[0])};
        if ((decider <  isovalue && msCase == 5) ||
            (decider >= isovalue && msCase == 10)) {
          std::swap(vertices[vertices.size()-1],vertices[vertices.size()-3]);
        }
      }
    }
  }
}
