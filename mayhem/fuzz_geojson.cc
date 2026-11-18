// GeoJSON document parser (geometry, features, properties, bounding boxes).
#include "support.h"

#include <CesiumVectorData/GeoJsonDocument.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CesiumVectorData::GeoJsonDocument::fromGeoJson(mayhem::asBytes(data, size));
  return 0;
}
