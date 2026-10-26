#include <array>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include <roofer/reconstruction/ArrangementDissolver.hpp>
#include <roofer/reconstruction/ElevationProvider.hpp>

namespace {
  roofer::Arrangement_2 square_arrangement() {
    using roofer::Point_2;
    using roofer::Segment_2;

    roofer::Arrangement_2 arrangement;
    std::array<Point_2, 4> points = {Point_2(0, 0), Point_2(10, 0),
                                     Point_2(10, 10), Point_2(0, 10)};
    for (size_t i = 0; i < points.size(); ++i) {
      CGAL::insert(arrangement,
                   Segment_2(points[i], points[(i + 1) % points.size()]));
    }

    for (auto face : arrangement.face_handles()) {
      if (face->is_unbounded()) continue;
      face->data().in_footprint = true;
      face->data().segid = 1;
    }
    return arrangement;
  }

  roofer::RasterTools::Raster empty_heightfield() {
    roofer::RasterTools::Raster raster(1, -1, 11, -1, 11);
    raster.prefill_arrays(roofer::RasterTools::MIN);
    return raster;
  }
}  // namespace

TEST_CASE("terrain clipping marks a boundary subface as ground") {
  auto arrangement = square_arrangement();
  for (auto face : arrangement.face_handles()) {
    if (!face->is_unbounded()) face->data().plane = roofer::Plane(1, 0, -1, 0);
  }

  auto elevation_provider =
      roofer::reconstruction::createElevationProvider(5.0F);
  auto dissolver = roofer::reconstruction::createArrangementDissolver();
  auto heightfield = empty_heightfield();
  dissolver->compute(
      arrangement, heightfield, *elevation_provider,
      {.dissolve_segment_edges = false, .dissolve_outside_footprint = false});

  size_t roof_faces = 0;
  size_t ground_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->is_unbounded()) continue;
    if (face->data().in_footprint) ++roof_faces;
    if (face->data().is_ground) {
      ++ground_faces;
      CHECK_FALSE(face->data().in_footprint);
      CHECK_FALSE(face->data().is_footprint_hole);
    }
  }
  CHECK(roof_faces == 1);
  CHECK(ground_faces == 1);
}

TEST_CASE("terrain clipping marks an enclosed subface as a footprint hole") {
  auto arrangement = square_arrangement();
  for (auto face : arrangement.face_handles()) {
    if (!face->is_unbounded()) face->data().plane = roofer::Plane(0, 0, 1, -5);
  }

  roofer::LinearRing terrain;
  terrain.insert(terrain.end(),
                 {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 10}});
  auto terrain_cdt = roofer::proj_tri_util::cdt_from_linearing(terrain);
  auto elevation_provider =
      roofer::reconstruction::createElevationProvider(terrain_cdt);
  auto dissolver = roofer::reconstruction::createArrangementDissolver();
  auto heightfield = empty_heightfield();
  dissolver->compute(
      arrangement, heightfield, *elevation_provider,
      {.dissolve_segment_edges = false, .dissolve_outside_footprint = false});

  size_t enclosed_ground_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->is_unbounded()) continue;
    if (face->data().is_ground && face->data().is_footprint_hole) {
      ++enclosed_ground_faces;
      CHECK_FALSE(face->data().in_footprint);
    }
  }
  CHECK(enclosed_ground_faces == 1);
}

TEST_CASE("a face without measured cells takes the height of its plane") {
  // The LoD 1.2/1.3 extrusion uses elevation_70p / elevation_97p as the face
  // height. A face whose cells carry no heightfield data used to be given the
  // raster nodata sentinel (-FLT_MAX) and was extruded far below ground.
  auto arrangement = square_arrangement();
  for (auto face : arrangement.face_handles()) {
    if (!face->is_unbounded()) face->data().plane = roofer::Plane(0, 0, 1, -7);
  }

  auto elevation_provider =
      roofer::reconstruction::createElevationProvider(0.0F);
  auto dissolver = roofer::reconstruction::createArrangementDissolver();
  roofer::RasterTools::Raster heightfield(1, -1, 11, -1, 11);
  heightfield.prefill_arrays(roofer::RasterTools::MAX);
  dissolver->compute(
      arrangement, heightfield, *elevation_provider,
      {.dissolve_segment_edges = false, .dissolve_outside_footprint = false});

  size_t roof_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->is_unbounded() || !face->data().in_footprint) continue;
    ++roof_faces;
    CHECK(face->data().pixel_count == 0);
    CHECK(face->data().data_coverage == 0.0F);
    CHECK(std::abs(face->data().elevation_70p - 7.0F) < 1e-3F);
    CHECK(std::abs(face->data().elevation_97p - 7.0F) < 1e-3F);
    CHECK(std::abs(face->data().elevation_50p - 7.0F) < 1e-3F);
  }
  CHECK(roof_faces == 1);
}
