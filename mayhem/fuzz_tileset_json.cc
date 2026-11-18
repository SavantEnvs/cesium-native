// 3D Tiles tileset.json and subtree (binary "subt" or JSON) readers, with external buffer resolution
// answered by an always-404 accessor.
#include "support.h"

#include <Cesium3DTilesReader/SubtreeFileReader.h>
#include <Cesium3DTilesReader/TilesetReader.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static CesiumAsync::AsyncSystem asyncSystem(
      std::make_shared<mayhem::InlineTaskProcessor>());
  static std::shared_ptr<CesiumAsync::IAssetAccessor> pAccessor =
      std::make_shared<mayhem::NotFoundAccessor>();

  const auto bytes = mayhem::asBytes(data, size);
  Cesium3DTilesReader::TilesetReader().readFromJson(bytes);
  Cesium3DTilesReader::SubtreeFileReader()
      .load(asyncSystem, pAccessor, "https://example.invalid/0.0.0.subtree", {}, bytes)
      .wait();
  return 0;
}
