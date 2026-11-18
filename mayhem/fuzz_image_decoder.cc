// Image decoding: PNG, JPEG, WebP, KTX2 (Basis Universal, raw and GPU-transcoded) and the stb fallback.
#include "support.h"

#include <CesiumImage/ImageDecoder.h>
#include <CesiumImage/Ktx2TranscodeTargets.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace CesiumImage;
  const auto bytes = mayhem::asBytes(data, size);

  ImageDecoder::readImage(bytes, Ktx2TranscodeTargets{});

  SupportedGpuCompressedPixelFormats formats{};
  formats.BC7_RGBA = true;
  formats.BC5_RG = true;
  formats.BC4_R = true;
  formats.BC3_RGBA = true;
  formats.BC1_RGB = true;
  ImageDecoder::readImage(bytes, Ktx2TranscodeTargets(formats, true));
  return 0;
}
