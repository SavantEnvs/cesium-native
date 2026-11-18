#!/usr/bin/env bash
#
# mayhem/test.sh: runs the upstream doctest suite and a set of known-answer probes, both built by
# mayhem/build.sh with normal flags. A no-op program prints no doctest summary and no probe output,
# so every check below fails for it.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

TESTS=build-test/cesium-native/CesiumNativeTests/cesium-native-tests
KAT=build-test/bin/kat
for exe in "$TESTS" "$KAT"; do
  [ -x "$exe" ] || { echo "missing $exe: mayhem/build.sh did not build it" >&2; exit 1; }
done

passed=0 failed=0 skipped=0

# 1) the upstream doctest suite: 657 test cases, ~3.7M assertions.
out="$(timeout 1500 "./$TESTS" 2>&1)"
summary="$(printf '%s\n' "$out" | grep -E '^\[doctest\] test cases:' | tail -1)"
if [[ "$summary" =~ test\ cases:\ +([0-9]+)\ \|\ +([0-9]+)\ passed\ \|\ +([0-9]+)\ failed\ \|\ +([0-9]+)\ skipped ]] \
   && [ "${BASH_REMATCH[1]}" -ge 600 ] && [ "${BASH_REMATCH[3]}" -eq 0 ] \
   && [ "${BASH_REMATCH[2]}" -eq "${BASH_REMATCH[1]}" ]; then
  passed=$(( passed + BASH_REMATCH[2] - BASH_REMATCH[4] ))
  skipped=$(( skipped + BASH_REMATCH[4] ))
else
  echo "doctest suite did not report a clean run: '${summary:-no summary}'" >&2
  printf '%s\n' "$out" | tail -30 >&2
  failed=$(( failed + 1 ))
fi

# 2) known-answer probes: parse upstream test files and compare the facts to fixed values.
check() {
  local name="$1" expected="$2"; shift 2
  local got
  got="$("./$KAT" "$@" 2>/dev/null | tr '\n' ' ')"
  if [ "$got" = "$expected" ]; then
    passed=$(( passed + 1 ))
  else
    echo "KAT $name: expected '$expected' got '$got'" >&2
    failed=$(( failed + 1 ))
  fi
}
check glb-cube       "errors=0 meshes=1 primitives=1 accessors=1 positions=36 " \
  glb CesiumGltfContent/test/data/cube.glb
check glb-indexed    "errors=0 meshes=1 primitives=1 accessors=2 positions=8 indices=36 " \
  glb CesiumGltfContent/test/data/cubeIndexed.glb
check b3dm-cube      "errors=0 meshes=1 primitives=1 accessors=3 positions=24 indices=36 " \
  tile Cesium3DTilesSelection/test/data/stacked-cubes/cube.b3dm
check pnts-quantized "errors=0 meshes=1 primitives=1 accessors=2 positions=8 " \
  tile Cesium3DTilesSelection/test/data/PointCloud/pointCloudQuantized.pnts
check geojson-equator "errors=0 objects=3 " \
  geojson CesiumVectorOverlays/test/data/equator.geojson
check terrain-normals "errors=0 meshes=1 primitives=1 accessors=3 positions=252 indices=1380 " \
  terrain Cesium3DTilesSelection/test/data/CesiumTerrainTileJson/tile.vertexnormals.terrain
check tileset-root   "errors=0 version=1.0 geometricError=240 children=4 " \
  tileset Cesium3DTilesSelection/test/data/Tileset/tileset.json

# the terrain layer.json reader, and the TMS / WMS XML documents read by the raster overlays
check layer-json     "errors=0 format=quantized-mesh-1.0 version=1.2.0 maxzoom=19 tiles=1 extensions=4 available=0 " \
  layer Cesium3DTilesSelection/test/data/CesiumTerrainTileJson/9_759_335/layer.json
check tms-logo       "provider=1 minLevel=0 maxLevel=4 tile=256x256 " \
  tms CesiumRasterOverlays/test/data/Cesium_Logo_Color/tilemapresource.xml
check tms-not-xml    "error=Unable to parse Tile map service XML document. " \
  tms Cesium3DTilesSelection/test/data/CesiumTerrainTileJson/9_759_335/layer.json

emit_ctrf "doctest+kat" "$passed" "$failed" "$skipped"
