// XML documents a raster overlay server answers with: the Tile Map Service `tilemapresource.xml` and
// the WMS GetCapabilities response. Driven through each overlay's own createTileProvider(), with an
// accessor that returns the fuzz bytes as the 200 response body of every request.
#include "support.h"

#include <CesiumRasterOverlays/CreateRasterOverlayTileProviderParameters.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/WebMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayLoadFailureDetails.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <exception>

namespace {

void createProvider(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAccessor,
    const CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>&
        pOverlay) {
  CesiumRasterOverlays::RasterOverlayExternals externals{
      .pAssetAccessor = pAccessor,
      .asyncSystem = asyncSystem};
  CesiumRasterOverlays::CreateRasterOverlayTileProviderParameters parameters{
      externals,
      pOverlay};
  // RasterOverlay::activate, the only production caller, turns an exception from createTileProvider
  // into a load failure; mirror it so only what the application cannot handle is reported.
  pOverlay->createTileProvider(parameters)
      .catchInMainThread(
          [](const std::exception&)
              -> CesiumRasterOverlays::RasterOverlay::CreateTileProviderResult {
            return nonstd::make_unexpected(
                CesiumRasterOverlays::RasterOverlayLoadFailureDetails{
                    CesiumRasterOverlays::RasterOverlayLoadType::Unknown,
                    nullptr,
                    "error"});
          })
      .waitInMainThread();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace CesiumRasterOverlays;
  static CesiumAsync::AsyncSystem asyncSystem(
      std::make_shared<mayhem::InlineTaskProcessor>());

  const std::shared_ptr<CesiumAsync::IAssetAccessor> pAccessor =
      std::make_shared<mayhem::FixedResponseAccessor>(
          uint16_t(200),
          std::vector<std::byte>(
              reinterpret_cast<const std::byte*>(data),
              reinterpret_cast<const std::byte*>(data) + size));

  createProvider(
      asyncSystem,
      pAccessor,
      new TileMapServiceRasterOverlay("tms", "https://example.invalid/tms/"));

  WebMapServiceRasterOverlayOptions wmsOptions;
  wmsOptions.layers = "a,b";
  createProvider(
      asyncSystem,
      pAccessor,
      new WebMapServiceRasterOverlay(
          "wms",
          "https://example.invalid/wms",
          {},
          wmsOptions));
  return 0;
}
