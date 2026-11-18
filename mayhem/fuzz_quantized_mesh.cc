// quantized-mesh-1.0 terrain tiles: header, vertex data, indices, extensions (water mask, metadata).
#include "support.h"

#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumQuantizedMeshTerrain/QuantizedMeshLoader.h>

#include <exception>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace CesiumQuantizedMeshTerrain;
  const auto bytes = mayhem::asBytes(data, size);
  const CesiumGeospatial::BoundingRegion region(
      CesiumGeospatial::GlobeRectangle::fromDegrees(0.0, 0.0, 1.0, 1.0),
      0.0,
      1000.0,
      CesiumGeospatial::Ellipsoid::WGS84);
  // LayerJsonTerrainLoader runs load() inside thenInWorkerThread, which stores an exception in the
  // future for the caller; only sanitizer reports and crashes it cannot handle are reported here.
  try {
    QuantizedMeshLoader::load(
        CesiumGeometry::QuadtreeTileID(1, 0, 0),
        region,
        "https://example.invalid/0/0/0.terrain",
        bytes,
        true,
        CesiumGeospatial::Ellipsoid::WGS84);
  } catch (const std::exception&) {
  }
  try {
    QuantizedMeshLoader::loadMetadata(
        bytes,
        CesiumGeometry::QuadtreeTileID(1, 0, 0));
  } catch (const std::exception&) {
  }
  return 0;
}
