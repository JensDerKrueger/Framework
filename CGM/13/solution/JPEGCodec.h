#pragma once

#include <Image.h>

#include <cstdint>
#include <string>
#include <vector>

namespace JPEG {

enum class ChromaMode {
  YCbCr444,
  YCbCr422,
  YCbCr420
};

enum class QuantizationPreset {
  JPEGStandard,
  Flat,
  HighFrequency
};

struct CompressionResult {
  Image reconstructed;
  std::vector<uint8_t> jpegBytes;
  uint32_t paddedWidth{0};
  uint32_t paddedHeight{0};
};

CompressionResult compressImage(const Image& source,
                                int quality,
                                ChromaMode chromaMode,
                                QuantizationPreset quantizationPreset);

bool saveJPEG(const std::string& filename, const std::vector<uint8_t>& jpegBytes);

const char* chromaModeName(ChromaMode chromaMode);
const char* quantizationPresetName(QuantizationPreset preset);

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
