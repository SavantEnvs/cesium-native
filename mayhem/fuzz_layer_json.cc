// Terrain layer.json (tilejson with `available` rectangles, extensions, parentUrl), as served by a
// quantized-mesh terrain server and read by Cesium3DTilesSelection's LayerJsonTerrainLoader.
#include "support.h"

#include <CesiumQuantizedMeshTerrain/LayerReader.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumQuantizedMeshTerrain::LayerReader().readFromJson(
      mayhem::asBytes(data, size));
  return 0;
}
