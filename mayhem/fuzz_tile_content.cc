// 3D Tiles tile content converters (b3dm, i3dm, pnts, cmpt, glb), dispatched on the 4-byte magic.
#include "support.h"

#include <Cesium3DTilesContent/GltfConverters.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <CesiumGeometry/Axis.h>
#include <CesiumGltfReader/GltfReader.h>

#include <glm/ext/matrix_double4x4.hpp>

#include <exception>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  Cesium3DTilesContent::registerAllTileContentTypes();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static CesiumAsync::AsyncSystem asyncSystem(
      std::make_shared<mayhem::InlineTaskProcessor>());
  static std::shared_ptr<CesiumAsync::IAssetAccessor> pAccessor =
      std::make_shared<mayhem::NotFoundAccessor>();

  Cesium3DTilesContent::AssetFetcher fetcher(
      asyncSystem,
      pAccessor,
      "https://example.invalid/tile/",
      glm::dmat4(1.0),
      {},
      CesiumGeometry::Axis::Y);
  CesiumGltfReader::GltfReaderOptions options;
  // TilesetContentManager catches an exception stored in the future (catchInMainThread), so only
  // sanitizer reports and crashes the application cannot handle are reported here.
  try {
    Cesium3DTilesContent::GltfConverters::convert(
        mayhem::asBytes(data, size),
        options,
        fetcher)
        .wait();
  } catch (const std::exception&) {
  }
  return 0;
}
