// Known-answer probe for mayhem/test.sh: parses one upstream test file with the normal (unsanitized)
// build and prints facts about the result, so the oracle can compare them to fixed expectations.
#include "support.h"

#include <Cesium3DTilesContent/GltfConverters.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <Cesium3DTilesReader/TilesetReader.h>
#include <CesiumGeometry/Axis.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGltf/Accessor.h>
#include <CesiumGltf/Mesh.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumQuantizedMeshTerrain/LayerReader.h>
#include <CesiumQuantizedMeshTerrain/QuantizedMeshLoader.h>
#include <CesiumRasterOverlays/CreateRasterOverlayTileProviderParameters.h>
#include <CesiumRasterOverlays/QuadtreeRasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/WebMapServiceRasterOverlay.h>
#include <CesiumVectorData/GeoJsonDocument.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::vector<std::byte> slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<std::byte> out(s.size());
  std::memcpy(out.data(), s.data(), s.size());
  return out;
}

void describe(const CesiumGltf::Model& m) {
  size_t prims = 0;
  for (const auto& mesh : m.meshes) {
    prims += mesh.primitives.size();
  }
  std::printf("meshes=%zu primitives=%zu accessors=%zu\n", m.meshes.size(), prims, m.accessors.size());
  if (!m.meshes.empty() && !m.meshes[0].primitives.empty()) {
    const auto& p = m.meshes[0].primitives[0];
    auto it = p.attributes.find("POSITION");
    if (it != p.attributes.end()) {
      std::printf("positions=%lld\n", (long long)m.accessors.at(size_t(it->second)).count);
    }
    if (p.indices >= 0) {
      std::printf("indices=%lld\n", (long long)m.accessors.at(size_t(p.indices)).count);
    }
  }
}
} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return 2;
  }
  const std::string kind = argv[1];
  const auto bytes = slurp(argv[2]);
  const std::span<const std::byte> span(bytes);

  if (kind == "glb") {
    auto r = CesiumGltfReader::GltfReader().readGltf(span);
    std::printf("errors=%zu\n", r.errors.size());
    if (r.model) describe(*r.model);
  } else if (kind == "tile") {
    Cesium3DTilesContent::registerAllTileContentTypes();
    CesiumAsync::AsyncSystem asyncSystem(std::make_shared<mayhem::InlineTaskProcessor>());
    Cesium3DTilesContent::AssetFetcher fetcher(
        asyncSystem, std::make_shared<mayhem::NotFoundAccessor>(), "https://example.invalid/",
        glm::dmat4(1.0), {}, CesiumGeometry::Axis::Y);
    auto r = Cesium3DTilesContent::GltfConverters::convert(span, {}, fetcher).wait();
    std::printf("errors=%zu\n", r.errors.errors.size());
    if (r.model) describe(*r.model);
  } else if (kind == "geojson") {
    auto r = CesiumVectorData::GeoJsonDocument::fromGeoJson(span);
    std::printf("errors=%zu\n", r.errors.errors.size());
    if (r.value) {
      size_t n = 0;
      for ([[maybe_unused]] const auto& o : r.value->rootObject) ++n;
      std::printf("objects=%zu\n", n);
    }
  } else if (kind == "terrain") {
    const CesiumGeospatial::BoundingRegion region(
        CesiumGeospatial::GlobeRectangle::fromDegrees(0.0, 0.0, 1.0, 1.0), 0.0, 1000.0,
        CesiumGeospatial::Ellipsoid::WGS84);
    auto r = CesiumQuantizedMeshTerrain::QuantizedMeshLoader::load(
        CesiumGeometry::QuadtreeTileID(1, 0, 0), region, "https://example.invalid/0.terrain",
        span, false, CesiumGeospatial::Ellipsoid::WGS84);
    std::printf("errors=%zu\n", r.errors.errors.size());
    if (r.model) describe(*r.model);
  } else if (kind == "tileset") {
    auto r = Cesium3DTilesReader::TilesetReader().readFromJson(span);
    std::printf("errors=%zu\n", r.errors.size());
    if (r.value) {
      std::printf("version=%s geometricError=%g children=%zu\n", r.value->asset.version.c_str(),
                  r.value->geometricError, r.value->root.children.size());
    }
  } else if (kind == "layer") {
    auto r = CesiumQuantizedMeshTerrain::LayerReader().readFromJson(span);
    std::printf("errors=%zu\n", r.errors.size());
    if (r.value) {
      std::printf("format=%s version=%s maxzoom=%lld tiles=%zu extensions=%zu available=%zu\n",
                  r.value->format.c_str(), r.value->version.c_str(), (long long)r.value->maxzoom,
                  r.value->tiles.size(), r.value->extensionsProperty.size(), r.value->available.size());
    }
  } else if (kind == "tms" || kind == "wms") {
    // The overlay's own createTileProvider(), fed `argv[2]` as the body of every response.
    CesiumAsync::AsyncSystem asyncSystem(std::make_shared<mayhem::InlineTaskProcessor>());
    CesiumRasterOverlays::RasterOverlayExternals externals{
        .pAssetAccessor = std::make_shared<mayhem::FixedResponseAccessor>(uint16_t(200), bytes),
        .asyncSystem = asyncSystem};
    CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay> pOverlay;
    if (kind == "tms") {
      pOverlay = new CesiumRasterOverlays::TileMapServiceRasterOverlay("tms", "https://example.invalid/tms/");
    } else {
      CesiumRasterOverlays::WebMapServiceRasterOverlayOptions options;
      options.layers = "a,b";
      pOverlay = new CesiumRasterOverlays::WebMapServiceRasterOverlay("wms", "https://example.invalid/wms", {}, options);
    }
    CesiumRasterOverlays::CreateRasterOverlayTileProviderParameters parameters{externals, pOverlay};
    auto r = pOverlay->createTileProvider(parameters).waitInMainThread();
    if (!r) {
      std::printf("error=%s\n", r.error().message.c_str());
    } else {
      const auto* pQuadtree =
          dynamic_cast<const CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider*>(r->get());
      std::printf("provider=1 minLevel=%u maxLevel=%u tile=%ux%u\n", pQuadtree->getMinimumLevel(),
                  pQuadtree->getMaximumLevel(), pQuadtree->getWidth(), pQuadtree->getHeight());
    }
  } else {
    return 2;
  }
  return 0;
}
