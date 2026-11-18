cmake_minimum_required(VERSION 3.25)
project(cesium-native-mayhem LANGUAGES C CXX)

# The upstream project, configured against the vcpkg prefix baked into the image.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. cesium-native)

option(MAYHEM_BUILD_KAT "Build the known-answer probe used by mayhem/test.sh instead of the fuzz harnesses" OFF)
if(MAYHEM_BUILD_KAT)
    add_executable(kat kat.cc)
    target_include_directories(kat PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../CesiumNativeTests/include)
    target_link_libraries(kat PRIVATE
        Cesium3DTilesContent Cesium3DTilesReader CesiumVectorData
        CesiumQuantizedMeshTerrain CesiumGltfReader CesiumGltfContent CesiumImage CesiumRasterOverlays)
    set_target_properties(kat PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    # Upstream derives CESIUM_NATIVE_DATA_DIR from CMAKE_SOURCE_DIR, which is mayhem/ in this wrapper project.
    if(TARGET cesium-native-tests)
        get_filename_component(MAYHEM_DATA_DIR "${CMAKE_CURRENT_LIST_DIR}/../data" ABSOLUTE)
        target_compile_options(cesium-native-tests PRIVATE
            "SHELL:-UCESIUM_NATIVE_DATA_DIR"
            "SHELL:-DCESIUM_NATIVE_DATA_DIR=\\\"${MAYHEM_DATA_DIR}\\\"")
    endif()
    return()
endif()

# QuantizedMeshLoader.cpp:171 reads uint32_t from an unaligned offset of every valid tile (benign
# on x86), so UBSan's alignment check would abort on all inputs. Relaxed for this library only.
target_compile_options(CesiumQuantizedMeshTerrain PRIVATE -fno-sanitize=alignment)

# The same benign pattern in the 3D Tiles content converters: CmptToGltfConverter.cpp:79 and the pnts
# feature-table reads load uint32_t / glm::vec3 from unaligned offsets in the tile's binary. Relaxed for
# this library only; every other check stays on.
target_compile_options(Cesium3DTilesContent PRIVATE -fno-sanitize=alignment)

set(MAYHEM_HARNESSES
    fuzz_gltf_reader
    fuzz_tile_content
    fuzz_geojson
    fuzz_quantized_mesh
    fuzz_tileset_json
    fuzz_image_decoder
    fuzz_layer_json
    fuzz_raster_overlay_xml)

# Every harness is linked twice: libFuzzer ($LIB_FUZZING_ENGINE) and the run-once standalone driver.
set(MAYHEM_FUZZ_LINK_FLAGS "$ENV{LIB_FUZZING_ENGINE}")
separate_arguments(MAYHEM_FUZZ_LINK_FLAGS)

add_library(mayhem_standalone_main OBJECT $ENV{STANDALONE_FUZZ_MAIN})

if(NOT MAYHEM_LSAN_OFF_SRC)
    set(MAYHEM_LSAN_OFF_SRC ${CMAKE_CURRENT_LIST_DIR}/lsan_off.cc)
endif()

foreach(h ${MAYHEM_HARNESSES})
    foreach(kind fuzz standalone)
        if(kind STREQUAL "fuzz")
            set(name ${h})
        else()
            set(name ${h}-standalone)
        endif()
        add_executable(${name} ${h}.cc ${MAYHEM_LSAN_OFF_SRC})
        target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/../CesiumNativeTests/include)
        target_link_libraries(${name} PRIVATE
            Cesium3DTilesContent Cesium3DTilesReader CesiumVectorData
            CesiumQuantizedMeshTerrain CesiumGltfReader CesiumGltfContent CesiumImage CesiumRasterOverlays)
        if(kind STREQUAL "fuzz")
            target_link_options(${name} PRIVATE ${MAYHEM_FUZZ_LINK_FLAGS})
        else()
            target_sources(${name} PRIVATE $<TARGET_OBJECTS:mayhem_standalone_main>)
        endif()
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    endforeach()
endforeach()
