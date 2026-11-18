// glTF / GLB reader: JSON, GLB framing, Draco, meshopt, KTX2/PNG/JPEG/WebP images, then ray picking
// over whatever model came out.
#include "support.h"

#include <CesiumGeometry/Ray.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfReader/GltfReader.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumGltfReader::GltfReader reader;
  CesiumGltfReader::GltfReaderResult result =
      reader.readGltf(mayhem::asBytes(data, size));
  if (result.model) {
    CesiumGltfContent::GltfUtilities::intersectRayGltfModel(
        CesiumGeometry::Ray(glm::dvec3(0.0, 0.0, 10.0), glm::dvec3(0.0, 0.0, -1.0)),
        *result.model,
        false);
  }
  return 0;
}
