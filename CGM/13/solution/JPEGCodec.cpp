#include "JPEGCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace JPEG {
namespace {

constexpr float pi = 3.14159265358979323846f;

constexpr std::array<uint8_t, 64> zigZag = {
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63
};

constexpr std::array<uint8_t, 64> luminanceQuantization = {
 16, 11, 10, 16, 24, 40, 51, 61,
 12, 12, 14, 19, 26, 58, 60, 55,
 14, 13, 16, 24, 40, 57, 69, 56,
 14, 17, 22, 29, 51, 87, 80, 62,
 18, 22, 37, 56, 68,109,103, 77,
 24, 35, 55, 64, 81,104,113, 92,
 49, 64, 78, 87,103,121,120,101,
 72, 92, 95, 98,112,100,103, 99
};

constexpr std::array<uint8_t, 64> chrominanceQuantization = {
 17, 18, 24, 47, 99, 99, 99, 99,
 18, 21, 26, 66, 99, 99, 99, 99,
 24, 26, 56, 99, 99, 99, 99, 99,
 47, 66, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99
};

const std::array<std::array<float, 8>, 8>& dctBasis() {
  static const std::array<std::array<float, 8>, 8> basis = [] {
    std::array<std::array<float, 8>, 8> result{};
    for (uint32_t frequency = 0; frequency < 8U; ++frequency) {
      for (uint32_t position = 0; position < 8U; ++position) {
        result[frequency][position] =
          std::cos(((2.0f * float(position) + 1.0f) * float(frequency) * pi) / 16.0f);
      }
    }
    return result;
  }();
  return basis;
}

constexpr std::array<uint8_t, 16> dcLuminanceBits = {
  0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

constexpr std::array<uint8_t, 12> dcLuminanceValues = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

constexpr std::array<uint8_t, 16> acLuminanceBits = {
  0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};

constexpr std::array<uint8_t, 162> acLuminanceValues = {
  0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,
  0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
  0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
  0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
  0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,
  0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
  0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,
  0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
  0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
  0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
  0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
  0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
  0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
  0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
  0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
  0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
  0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,
  0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
  0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
  0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
  0xf9,0xfa
};

constexpr std::array<uint8_t, 16> dcChrominanceBits = {
  0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

constexpr std::array<uint8_t, 12> dcChrominanceValues = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

constexpr std::array<uint8_t, 16> acChrominanceBits = {
  0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
};

constexpr std::array<uint8_t, 162> acChrominanceValues = {
  0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,
  0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
  0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
  0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
  0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,
  0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
  0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,
  0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
  0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,
  0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
  0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,
  0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
  0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,
  0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
  0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,
  0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
  0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,
  0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
  0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,
  0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
  0xf9,0xfa
};

struct Channel {
  uint32_t width{0};
  uint32_t height{0};
  std::vector<float> samples;

  Channel() = default;

  Channel(const uint32_t w, const uint32_t h, const float value = 0.0f) :
  width{w},
  height{h},
  samples(size_t(w) * size_t(h), value) {
  }

  float& at(const uint32_t x, const uint32_t y) {
    return samples[size_t(y) * width + x];
  }

  float at(const uint32_t x, const uint32_t y) const {
    return samples[size_t(y) * width + x];
  }
};

struct YCbCrImage {
  Channel y;
  Channel cb;
  Channel cr;
};

struct HuffmanCode {
  uint16_t code{0};
  uint8_t length{0};
};

struct HuffmanTable {
  std::array<HuffmanCode, 256> codes{};
};

struct BitWriter {
  std::vector<uint8_t>& output;
  uint32_t bitBuffer{0};
  uint8_t bitCount{0};

  explicit BitWriter(std::vector<uint8_t>& target) :
  output{target} {
  }

  void writeByte(const uint8_t value) {
    output.push_back(value);
    if (value == 0xff) {
      output.push_back(0x00);
    }
  }

  void writeBits(const uint16_t bits, const uint8_t count) {
    bitBuffer = (bitBuffer << count) | bits;
    bitCount = uint8_t(bitCount + count);

    while (bitCount >= 8) {
      const uint8_t value = uint8_t((bitBuffer >> (bitCount - 8)) & 0xffU);
      writeByte(value);
      bitCount = uint8_t(bitCount - 8);
    }
  }

  void flush() {
    if (bitCount == 0) {
      return;
    }

    const uint8_t padding = uint8_t(8 - bitCount);
    const uint16_t fillBits = uint16_t((1U << padding) - 1U);
    writeBits(fillBits, padding);
  }
};

uint32_t roundUp(const uint32_t value, const uint32_t multiple) {
  return ((value + multiple - 1U) / multiple) * multiple;
}

uint8_t clampByte(const float value) {
  return uint8_t(std::clamp(std::lround(value), long{0}, long{255}));
}

std::array<uint8_t, 64> scaledQuantizationTable(const std::array<uint8_t, 64>& base,
                                                const int rawQuality) {
  const int quality = std::clamp(rawQuality, 1, 100);
  const int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
  std::array<uint8_t, 64> result{};

  for (size_t i = 0; i < base.size(); ++i) {
    const int value = (int(base[i]) * scale + 50) / 100;
    result[i] = uint8_t(std::clamp(value, 1, 255));
  }

  return result;
}

std::array<uint8_t, 64> flatQuantizationBase(const uint8_t value) {
  std::array<uint8_t, 64> result{};
  result.fill(value);
  return result;
}

std::array<uint8_t, 64> highFrequencyBase(const std::array<uint8_t, 64>& standard) {
  std::array<uint8_t, 64> result{};

  for (uint32_t y = 0; y < 8U; ++y) {
    for (uint32_t x = 0; x < 8U; ++x) {
      const size_t index = y * 8U + x;
      const float frequency = float(x + y) / 14.0f;
      const float scale = 0.65f + 0.35f * frequency;
      result[index] = uint8_t(std::clamp(std::lround(float(standard[index]) * scale), long{1}, long{255}));
    }
  }

  return result;
}

std::pair<std::array<uint8_t, 64>, std::array<uint8_t, 64>>
quantizationTables(const int quality, const QuantizationPreset preset) {
  switch (preset) {
    case QuantizationPreset::Flat:
      return {
        scaledQuantizationTable(flatQuantizationBase(32), quality),
        scaledQuantizationTable(flatQuantizationBase(32), quality)
      };
    case QuantizationPreset::HighFrequency:
      return {
        scaledQuantizationTable(highFrequencyBase(luminanceQuantization), quality),
        scaledQuantizationTable(highFrequencyBase(chrominanceQuantization), quality)
      };
    case QuantizationPreset::JPEGStandard:
      return {
        scaledQuantizationTable(luminanceQuantization, quality),
        scaledQuantizationTable(chrominanceQuantization, quality)
      };
  }

  return {
    scaledQuantizationTable(luminanceQuantization, quality),
    scaledQuantizationTable(chrominanceQuantization, quality)
  };
}

YCbCrImage convertToYCbCr(const Image& source,
                          const uint32_t paddedWidth,
                          const uint32_t paddedHeight) {
  YCbCrImage result{
    Channel{paddedWidth, paddedHeight},
    Channel{paddedWidth, paddedHeight},
    Channel{paddedWidth, paddedHeight}
  };

  for (uint32_t y = 0; y < paddedHeight; ++y) {
    const uint32_t sourceY = std::min(y, source.height - 1U);
    for (uint32_t x = 0; x < paddedWidth; ++x) {
      const uint32_t sourceX = std::min(x, source.width - 1U);
      const float r = float(source.getValue(sourceX, sourceY, 0));
      const float g = float(source.getValue(sourceX, sourceY, std::min<uint8_t>(1, source.componentCount - 1)));
      const float b = float(source.getValue(sourceX, sourceY, std::min<uint8_t>(2, source.componentCount - 1)));

      result.y.at(x, y) = 0.299f * r + 0.587f * g + 0.114f * b;
      result.cb.at(x, y) = -0.168736f * r - 0.331264f * g + 0.5f * b + 128.0f;
      result.cr.at(x, y) = 0.5f * r - 0.418688f * g - 0.081312f * b + 128.0f;
    }
  }

  return result;
}

Channel downsampleChroma420(const Channel& fullResolutionChroma) {
  Channel result{fullResolutionChroma.width / 2U, fullResolutionChroma.height / 2U};

  for (uint32_t y = 0; y < result.height; ++y) {
    for (uint32_t x = 0; x < result.width; ++x) {
      const uint32_t sourceX = x * 2U;
      const uint32_t sourceY = y * 2U;
      const float sum =
        fullResolutionChroma.at(sourceX + 0U, sourceY + 0U) +
        fullResolutionChroma.at(sourceX + 1U, sourceY + 0U) +
        fullResolutionChroma.at(sourceX + 0U, sourceY + 1U) +
        fullResolutionChroma.at(sourceX + 1U, sourceY + 1U);
      result.at(x, y) = sum * 0.25f;
    }
  }

  return result;
}

Channel downsampleChroma422(const Channel& fullResolutionChroma) {
  Channel result{fullResolutionChroma.width / 2U, fullResolutionChroma.height};

  for (uint32_t y = 0; y < result.height; ++y) {
    for (uint32_t x = 0; x < result.width; ++x) {
      const uint32_t sourceX = x * 2U;
      result.at(x, y) =
        (fullResolutionChroma.at(sourceX + 0U, y) +
         fullResolutionChroma.at(sourceX + 1U, y)) * 0.5f;
    }
  }

  return result;
}

Channel upsampleHorizontal2x(const Channel& halfWidthChroma,
                             const uint32_t targetWidth,
                             const uint32_t targetHeight) {
  Channel result{targetWidth, targetHeight};

  for (uint32_t y = 0; y < targetHeight; ++y) {
    for (uint32_t x = 0; x < targetWidth; ++x) {
      result.at(x, y) = halfWidthChroma.at(x / 2U, y);
    }
  }

  return result;
}

Channel upsampleNearest2x(const Channel& halfResolutionChroma,
                          const uint32_t targetWidth,
                          const uint32_t targetHeight) {
  Channel result{targetWidth, targetHeight};

  for (uint32_t y = 0; y < targetHeight; ++y) {
    for (uint32_t x = 0; x < targetWidth; ++x) {
      result.at(x, y) = halfResolutionChroma.at(x / 2U, y / 2U);
    }
  }

  return result;
}

std::array<float, 64> computeDCTBlock(const Channel& channel,
                                      const uint32_t blockX,
                                      const uint32_t blockY) {
  std::array<float, 64> coefficients{};
  const auto& basis = dctBasis();

  for (uint32_t v = 0; v < 8U; ++v) {
    for (uint32_t u = 0; u < 8U; ++u) {
      float sum = 0.0f;

      for (uint32_t y = 0; y < 8U; ++y) {
        for (uint32_t x = 0; x < 8U; ++x) {
          const float sample = channel.at(blockX + x, blockY + y) - 128.0f;
          sum += sample * basis[u][x] * basis[v][y];
        }
      }

      const float alphaU = u == 0U ? 1.0f / std::sqrt(2.0f) : 1.0f;
      const float alphaV = v == 0U ? 1.0f / std::sqrt(2.0f) : 1.0f;
      coefficients[v * 8U + u] = 0.25f * alphaU * alphaV * sum;
    }
  }

  return coefficients;
}

std::array<int16_t, 64> quantizeBlock(const std::array<float, 64>& coefficients,
                                      const std::array<uint8_t, 64>& quantization) {
  std::array<int16_t, 64> result{};

  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = int16_t(std::lround(coefficients[i] / float(quantization[i])));
  }

  return result;
}

std::array<int16_t, 64> zigZagBlock(const std::array<int16_t, 64>& block) {
  std::array<int16_t, 64> result{};

  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = block[zigZag[i]];
  }

  return result;
}

void inverseDCTBlock(const std::array<int16_t, 64>& quantized,
                     const std::array<uint8_t, 64>& quantization,
                     Channel& target,
                     const uint32_t blockX,
                     const uint32_t blockY) {
  std::array<float, 64> coefficients{};
  const auto& basis = dctBasis();
  for (size_t i = 0; i < coefficients.size(); ++i) {
    coefficients[i] = float(quantized[i]) * float(quantization[i]);
  }

  for (uint32_t y = 0; y < 8U; ++y) {
    for (uint32_t x = 0; x < 8U; ++x) {
      float sum = 0.0f;

      for (uint32_t v = 0; v < 8U; ++v) {
        for (uint32_t u = 0; u < 8U; ++u) {
          const float alphaU = u == 0U ? 1.0f / std::sqrt(2.0f) : 1.0f;
          const float alphaV = v == 0U ? 1.0f / std::sqrt(2.0f) : 1.0f;
          sum += alphaU * alphaV * coefficients[v * 8U + u] * basis[u][x] * basis[v][y];
        }
      }

      target.at(blockX + x, blockY + y) = std::clamp(0.25f * sum + 128.0f, 0.0f, 255.0f);
    }
  }
}

HuffmanTable buildHuffmanTable(const std::array<uint8_t, 16>& bits,
                               const uint8_t* values,
                               const size_t valueCount) {
  HuffmanTable table{};
  uint16_t code = 0;
  size_t valueIndex = 0;

  for (uint8_t length = 1; length <= 16; ++length) {
    const uint8_t count = bits[length - 1U];
    for (uint8_t i = 0; i < count; ++i) {
      if (valueIndex >= valueCount) {
        throw std::runtime_error("Invalid Huffman table");
      }

      table.codes[values[valueIndex]] = HuffmanCode{code, length};
      ++code;
      ++valueIndex;
    }
    code = uint16_t(code << 1U);
  }

  return table;
}

uint8_t coefficientCategory(const int value) {
  if (value == 0) {
    return 0;
  }

  uint32_t magnitude = uint32_t(std::abs(value));
  uint8_t category = 0;
  while (magnitude != 0U) {
    ++category;
    magnitude >>= 1U;
  }

  return category;
}

uint16_t coefficientBits(const int value, const uint8_t category) {
  if (value >= 0) {
    return uint16_t(value);
  }

  return uint16_t(value + ((1 << category) - 1));
}

void writeHuffmanSymbol(BitWriter& writer,
                        const HuffmanTable& table,
                        const uint8_t symbol) {
  const HuffmanCode entry = table.codes[symbol];
  writer.writeBits(entry.code, entry.length);
}

void encodeBlock(BitWriter& writer,
                 const std::array<int16_t, 64>& naturalBlock,
                 int& previousDC,
                 const HuffmanTable& dcTable,
                 const HuffmanTable& acTable) {
  const std::array<int16_t, 64> block = zigZagBlock(naturalBlock);
  const int dcDiff = int(block[0]) - previousDC;
  previousDC = int(block[0]);

  const uint8_t dcCategory = coefficientCategory(dcDiff);
  writeHuffmanSymbol(writer, dcTable, dcCategory);
  if (dcCategory != 0) {
    writer.writeBits(coefficientBits(dcDiff, dcCategory), dcCategory);
  }

  uint8_t zeroRun = 0;
  for (size_t i = 1; i < block.size(); ++i) {
    const int value = int(block[i]);

    if (value == 0) {
      ++zeroRun;
      continue;
    }

    while (zeroRun > 15) {
      writeHuffmanSymbol(writer, acTable, 0xf0);
      zeroRun = uint8_t(zeroRun - 16);
    }

    const uint8_t acCategory = coefficientCategory(value);
    const uint8_t symbol = uint8_t((zeroRun << 4U) | acCategory);
    writeHuffmanSymbol(writer, acTable, symbol);
    writer.writeBits(coefficientBits(value, acCategory), acCategory);
    zeroRun = 0;
  }

  if (zeroRun != 0) {
    writeHuffmanSymbol(writer, acTable, 0x00);
  }
}

void appendByte(std::vector<uint8_t>& output, const uint8_t value) {
  output.push_back(value);
}

void appendWord(std::vector<uint8_t>& output, const uint16_t value) {
  output.push_back(uint8_t(value >> 8U));
  output.push_back(uint8_t(value & 0xffU));
}

void appendMarker(std::vector<uint8_t>& output, const uint8_t marker) {
  appendByte(output, 0xff);
  appendByte(output, marker);
}

void appendAPP0(std::vector<uint8_t>& output) {
  appendMarker(output, 0xe0);
  appendWord(output, 16);
  appendByte(output, 'J');
  appendByte(output, 'F');
  appendByte(output, 'I');
  appendByte(output, 'F');
  appendByte(output, 0);
  appendByte(output, 1);
  appendByte(output, 1);
  appendByte(output, 0);
  appendWord(output, 1);
  appendWord(output, 1);
  appendByte(output, 0);
  appendByte(output, 0);
}

void appendDQT(std::vector<uint8_t>& output,
               const std::array<uint8_t, 64>& qY,
               const std::array<uint8_t, 64>& qC) {
  appendMarker(output, 0xdb);
  appendWord(output, 2 + 65 + 65);
  appendByte(output, 0);
  for (uint8_t index : zigZag) {
    appendByte(output, qY[index]);
  }

  appendByte(output, 1);
  for (uint8_t index : zigZag) {
    appendByte(output, qC[index]);
  }
}

void appendSOF0(std::vector<uint8_t>& output,
                const uint32_t width,
                const uint32_t height,
                const ChromaMode chromaMode) {
  appendMarker(output, 0xc0);
  appendWord(output, 17);
  appendByte(output, 8);
  appendWord(output, uint16_t(height));
  appendWord(output, uint16_t(width));
  appendByte(output, 3);

  appendByte(output, 1);
  if (chromaMode == ChromaMode::YCbCr420) {
    appendByte(output, 0x22);
  } else if (chromaMode == ChromaMode::YCbCr422) {
    appendByte(output, 0x21);
  } else {
    appendByte(output, 0x11);
  }
  appendByte(output, 0);

  appendByte(output, 2);
  appendByte(output, 0x11);
  appendByte(output, 1);

  appendByte(output, 3);
  appendByte(output, 0x11);
  appendByte(output, 1);
}

template <size_t Count>
void appendDHT(std::vector<uint8_t>& output,
               const uint8_t tableClass,
               const uint8_t tableId,
               const std::array<uint8_t, 16>& bits,
               const std::array<uint8_t, Count>& values) {
  appendMarker(output, 0xc4);
  appendWord(output, uint16_t(2 + 1 + 16 + values.size()));
  appendByte(output, uint8_t((tableClass << 4U) | tableId));
  for (uint8_t count : bits) {
    appendByte(output, count);
  }
  for (uint8_t value : values) {
    appendByte(output, value);
  }
}

void appendSOS(std::vector<uint8_t>& output) {
  appendMarker(output, 0xda);
  appendWord(output, 12);
  appendByte(output, 3);
  appendByte(output, 1);
  appendByte(output, 0x00);
  appendByte(output, 2);
  appendByte(output, 0x11);
  appendByte(output, 3);
  appendByte(output, 0x11);
  appendByte(output, 0);
  appendByte(output, 63);
  appendByte(output, 0);
}

std::array<int16_t, 64> quantizedBlock(const Channel& channel,
                                       const uint32_t blockX,
                                       const uint32_t blockY,
                                       const std::array<uint8_t, 64>& quantization) {
  return quantizeBlock(computeDCTBlock(channel, blockX, blockY), quantization);
}

std::vector<uint8_t> encodeJPEGStream(const YCbCrImage& ycbcr,
                                      const uint32_t imageWidth,
                                      const uint32_t imageHeight,
                                      const ChromaMode chromaMode,
                                      const std::array<uint8_t, 64>& qY,
                                      const std::array<uint8_t, 64>& qC) {
  std::vector<uint8_t> output;
  output.reserve(size_t(imageWidth) * imageHeight / 2U);

  appendMarker(output, 0xd8);
  appendAPP0(output);
  appendDQT(output, qY, qC);
  appendSOF0(output, imageWidth, imageHeight, chromaMode);
  appendDHT(output, 0, 0, dcLuminanceBits, dcLuminanceValues);
  appendDHT(output, 1, 0, acLuminanceBits, acLuminanceValues);
  appendDHT(output, 0, 1, dcChrominanceBits, dcChrominanceValues);
  appendDHT(output, 1, 1, acChrominanceBits, acChrominanceValues);
  appendSOS(output);

  const HuffmanTable dcY = buildHuffmanTable(dcLuminanceBits,
                                             dcLuminanceValues.data(),
                                             dcLuminanceValues.size());
  const HuffmanTable acY = buildHuffmanTable(acLuminanceBits,
                                             acLuminanceValues.data(),
                                             acLuminanceValues.size());
  const HuffmanTable dcC = buildHuffmanTable(dcChrominanceBits,
                                             dcChrominanceValues.data(),
                                             dcChrominanceValues.size());
  const HuffmanTable acC = buildHuffmanTable(acChrominanceBits,
                                             acChrominanceValues.data(),
                                             acChrominanceValues.size());

  BitWriter writer{output};
  int previousY = 0;
  int previousCb = 0;
  int previousCr = 0;

  if (chromaMode == ChromaMode::YCbCr420) {
    for (uint32_t mcuY = 0; mcuY < ycbcr.y.height; mcuY += 16U) {
      for (uint32_t mcuX = 0; mcuX < ycbcr.y.width; mcuX += 16U) {
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX,      mcuY,      qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX + 8U, mcuY,      qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX,      mcuY + 8U, qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX + 8U, mcuY + 8U, qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.cb, mcuX / 2U, mcuY / 2U, qC), previousCb, dcC, acC);
        encodeBlock(writer, quantizedBlock(ycbcr.cr, mcuX / 2U, mcuY / 2U, qC), previousCr, dcC, acC);
      }
    }
  } else if (chromaMode == ChromaMode::YCbCr422) {
    for (uint32_t mcuY = 0; mcuY < ycbcr.y.height; mcuY += 8U) {
      for (uint32_t mcuX = 0; mcuX < ycbcr.y.width; mcuX += 16U) {
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX,      mcuY, qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.y, mcuX + 8U, mcuY, qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.cb, mcuX / 2U, mcuY, qC), previousCb, dcC, acC);
        encodeBlock(writer, quantizedBlock(ycbcr.cr, mcuX / 2U, mcuY, qC), previousCr, dcC, acC);
      }
    }
  } else {
    for (uint32_t blockY = 0; blockY < ycbcr.y.height; blockY += 8U) {
      for (uint32_t blockX = 0; blockX < ycbcr.y.width; blockX += 8U) {
        encodeBlock(writer, quantizedBlock(ycbcr.y, blockX, blockY, qY), previousY,  dcY, acY);
        encodeBlock(writer, quantizedBlock(ycbcr.cb, blockX, blockY, qC), previousCb, dcC, acC);
        encodeBlock(writer, quantizedBlock(ycbcr.cr, blockX, blockY, qC), previousCr, dcC, acC);
      }
    }
  }

  writer.flush();
  appendMarker(output, 0xd9);
  return output;
}

Channel reconstructChannel(const Channel& source,
                           const std::array<uint8_t, 64>& quantization) {
  Channel result{source.width, source.height};

  for (uint32_t y = 0; y < source.height; y += 8U) {
    for (uint32_t x = 0; x < source.width; x += 8U) {
      const std::array<int16_t, 64> block = quantizedBlock(source, x, y, quantization);
      inverseDCTBlock(block, quantization, result, x, y);
    }
  }

  return result;
}

Image convertToRGB(const Channel& yChannel,
                   const Channel& cbChannel,
                   const Channel& crChannel,
                   const uint32_t width,
                   const uint32_t height) {
  Image image{width, height, 3};

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const float yy = yChannel.at(x, y);
      const float cb = cbChannel.at(x, y) - 128.0f;
      const float cr = crChannel.at(x, y) - 128.0f;

      const float r = yy + 1.402f * cr;
      const float g = yy - 0.344136f * cb - 0.714136f * cr;
      const float b = yy + 1.772f * cb;

      image.setValue(x, y, 0, clampByte(r));
      image.setValue(x, y, 1, clampByte(g));
      image.setValue(x, y, 2, clampByte(b));
    }
  }

  return image;
}

} // namespace

CompressionResult compressImage(const Image& source,
                                const int quality,
                                const ChromaMode chromaMode,
                                const QuantizationPreset quantizationPreset) {
  const uint32_t mcuWidth = chromaMode == ChromaMode::YCbCr444 ? 8U : 16U;
  const uint32_t mcuHeight = chromaMode == ChromaMode::YCbCr420 ? 16U : 8U;
  const uint32_t paddedWidth = roundUp(source.width, mcuWidth);
  const uint32_t paddedHeight = roundUp(source.height, mcuHeight);
  const auto [qY, qC] = quantizationTables(quality, quantizationPreset);

  YCbCrImage ycbcr = convertToYCbCr(source, paddedWidth, paddedHeight);
  if (chromaMode == ChromaMode::YCbCr422) {
    ycbcr.cb = downsampleChroma422(ycbcr.cb);
    ycbcr.cr = downsampleChroma422(ycbcr.cr);
  } else if (chromaMode == ChromaMode::YCbCr420) {
    ycbcr.cb = downsampleChroma420(ycbcr.cb);
    ycbcr.cr = downsampleChroma420(ycbcr.cr);
  }

  const std::vector<uint8_t> jpegBytes = encodeJPEGStream(ycbcr,
                                                         source.width,
                                                         source.height,
                                                         chromaMode,
                                                         qY,
                                                         qC);

  const Channel reconstructedY = reconstructChannel(ycbcr.y, qY);
  const Channel reconstructedCbSmall = reconstructChannel(ycbcr.cb, qC);
  const Channel reconstructedCrSmall = reconstructChannel(ycbcr.cr, qC);
  const Channel reconstructedCb = chromaMode == ChromaMode::YCbCr420
    ? upsampleNearest2x(reconstructedCbSmall, paddedWidth, paddedHeight)
    : chromaMode == ChromaMode::YCbCr422
      ? upsampleHorizontal2x(reconstructedCbSmall, paddedWidth, paddedHeight)
      : reconstructedCbSmall;
  const Channel reconstructedCr = chromaMode == ChromaMode::YCbCr420
    ? upsampleNearest2x(reconstructedCrSmall, paddedWidth, paddedHeight)
    : chromaMode == ChromaMode::YCbCr422
      ? upsampleHorizontal2x(reconstructedCrSmall, paddedWidth, paddedHeight)
      : reconstructedCrSmall;

  return {
    convertToRGB(reconstructedY, reconstructedCb, reconstructedCr, source.width, source.height),
    jpegBytes,
    paddedWidth,
    paddedHeight
  };
}

bool saveJPEG(const std::string& filename, const std::vector<uint8_t>& jpegBytes) {
  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    return false;
  }

  file.write(reinterpret_cast<const char*>(jpegBytes.data()),
             std::streamsize(jpegBytes.size()));
  return bool(file);
}

const char* chromaModeName(const ChromaMode chromaMode) {
  switch (chromaMode) {
    case ChromaMode::YCbCr444:
      return "4:4:4";
    case ChromaMode::YCbCr422:
      return "4:2:2";
    case ChromaMode::YCbCr420:
      return "4:2:0";
  }

  return "unknown";
}

const char* quantizationPresetName(const QuantizationPreset preset) {
  switch (preset) {
    case QuantizationPreset::JPEGStandard:
      return "JPEG standard";
    case QuantizationPreset::Flat:
      return "flat";
    case QuantizationPreset::HighFrequency:
      return "detail preserving";
  }

  return "unknown";
}

}

/*
 Copyright (c) 2026 Computer Graphics and Visualization Group, University of
 Duisburg-Essen

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in the
 Software without restriction, including without limitation the rights to use, copy,
 modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
 to permit persons to whom the Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be included in all copies
 or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
