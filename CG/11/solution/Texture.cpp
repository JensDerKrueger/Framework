#include <iostream>
#include <cmath>

#include "Texture.h"
#include <stb_image.h>

Texture::Texture(uint32_t width, uint32_t height) :
  Texture(width, height, FilterMode::BILINEAR)
{
}

Texture::Texture(uint32_t width, uint32_t height, FilterMode filterMode) :
  Texture(width, height, filterMode, BorderMode::REPEAT)
{
}

Texture::Texture(uint32_t width, uint32_t height, FilterMode filterMode,
                 BorderMode borderMode) :
  width(width),
  height(height),
  filterMode(filterMode),
  borderModeU(borderMode),
  borderModeV(borderMode)
{
	borderColor = Vec3{ 0.0f, 0.0f, 0.0f };
	data = std::make_unique<Image>(width, height, 3);
}

Texture::Texture(const std::string& filename) :
  Texture(filename, FilterMode::BILINEAR)
{
}

Texture::Texture(const std::string& filename, FilterMode filterMode) :
  Texture(filename, filterMode, BorderMode::REPEAT)
{
}

Texture::Texture(const std::string& filename, FilterMode filterMode,
                 BorderMode borderMode) :
  filterMode(filterMode),
  borderModeU(borderMode),
  borderModeV(borderMode),
  borderColor(Vec3{0,0,0})
{
	stbi_set_flip_vertically_on_load(false);
	
	int width, height, nrComponents;
	stbi_uc* image_data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
	if(image_data) {
		this->width = uint32_t(width);
		this->height = uint32_t(height);
		this->borderModeU = borderMode;
		this->borderModeV = borderMode;
		this->filterMode = filterMode;

		data = std::make_unique<Image>(width, height, nrComponents, std::vector<uint8_t>{image_data, image_data + (width * height * nrComponents) });
		
		stbi_image_free(image_data);
	} else {
		std::cerr << "Texture failed to load at path: " << filename << std::endl;
		stbi_image_free(image_data);
	}
}

Texture Texture::genCheckerboardTexture(uint32_t width, uint32_t height) {
	Texture checkerboard(width, height, FilterMode::NEAREST, BorderMode::REPEAT);

	float lineValue = 0;
	for (uint32_t y = 0; y < height; y++) {
		float value = lineValue;
		for (uint32_t x = 0; x < width; ++x) {
			checkerboard.data->setNormalizedValue(x, y, 0, value);
			checkerboard.data->setNormalizedValue(x, y, 1, value);
			checkerboard.data->setNormalizedValue(x, y, 2, value);
			value = 1 - value;
		}
		lineValue = 1 - lineValue;
	}

	return checkerboard;
}

// max is already out of range since coordinate indices
// range from 0 to width/height - 1
static uint32_t handleBorderCoordinate(int pixelCoord, uint32_t max,
                                       BorderMode bordermode) {
	switch (bordermode) {
    case BorderMode::CLAMP_TO_EDGE:
      if (pixelCoord < 0)
        pixelCoord = 0;
      if (pixelCoord >= int(max))
        pixelCoord = int(max) - 1;
      break;
    case BorderMode::REPEAT:
      pixelCoord = pixelCoord % int(max);
      if (pixelCoord < 0)
        pixelCoord += int(max);
      break;
    case BorderMode::MIRRORED_REPEAT:
      pixelCoord = pixelCoord % (2 * int(max));
      if (pixelCoord < 0)
        pixelCoord += int(max) * 2;
      if (pixelCoord >= int(max))
        pixelCoord = (int(max) * 2 - 1) - pixelCoord;
      break;
    case BorderMode::CLAMP_TO_BORDER:
      // nothing todo here, border color must have been handled outside
      break;
	}
	return uint32_t(pixelCoord);
}

Vec3 Texture::sample(int pixelCoordX, int pixelCoordY) const {
	if (borderModeU == BorderMode::CLAMP_TO_BORDER &&
      (pixelCoordX < 0 || pixelCoordX >= int(width))) return borderColor;
	if (borderModeV== BorderMode::CLAMP_TO_BORDER &&
      (pixelCoordY < 0 || pixelCoordY >= int(height))) return borderColor;

	const uint32_t sampleCoordX = handleBorderCoordinate(pixelCoordX, width,
                                                       borderModeU);
  const uint32_t sampleCoordY = handleBorderCoordinate(pixelCoordY, height,
                                                       borderModeV);

	Vec3 textureColor{};
	textureColor[0] = data->getValue(sampleCoordX, sampleCoordY, 0) / 255.0f;
	textureColor[1] = data->getValue(sampleCoordX, sampleCoordY, 1) / 255.0f;
	textureColor[2] = data->getValue(sampleCoordX, sampleCoordY, 2) / 255.0f;
	return textureColor;
}

Vec3 Texture::sample(const TextureCoordinates& texCoords) const {
	const float dx = texCoords.u * width;
	const float dy = texCoords.v * height;

	switch (filterMode) {
    case FilterMode::NEAREST: {
      const int nx = static_cast<int>(std::floor(dx + 0.5f));
      const int ny = static_cast<int>(std::floor(dy + 0.5f));
      return sample(nx, ny);
    }
    case FilterMode::BILINEAR: {
      const int fx = (int)floor(dx);
      const int fy = (int)floor(dy);
      const int cx = (int)ceilf(dx);
      const int cy = (int)ceilf(dy);

      const Vec3 fxfy = sample(fx, fy);
      const Vec3 fxcy = sample(fx, cy);
      const Vec3 cxfy = sample(cx, fy);
      const Vec3 cxcy = sample(cx, cy);

      const float interpX = fabsf(dx - fx);
      const float interpY = fabsf(dy - fy);

      const Vec3 a = fxfy * (1.0f - interpX) + cxfy * interpX;
      const Vec3 b = fxcy * (1.0f - interpX) + cxcy * interpX;
      return a * (1.0f - interpY) + b * interpY;
    }
    default:
      std::cout << "Specified bordermode cannot be handled..." << std::endl;
      return {};
    }
}

void Texture::setBorderMode(BorderMode borderMode) {
	borderModeU = borderMode;
	borderModeV = borderMode;
}

void Texture::setBorderModeU(BorderMode borderMode) {
	borderModeU = borderMode;
}

void Texture::setBorderModeV(BorderMode borderMode) {
	borderModeV = borderMode;
}

void Texture::setBorderColor(const Vec3& borderColor) {
	this->borderColor = borderColor;
}
