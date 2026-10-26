// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

#include <array>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include <roofer/reconstruction/ArrangementOptimiser.hpp>

namespace {
  // A 10 x 10 footprint split by a vertical edge at x = 5 into a west and an
  // east face.
  roofer::Arrangement_2 split_square() {
    using roofer::Point_2;
    using roofer::Segment_2;

    roofer::Arrangement_2 arrangement;
    std::array<Point_2, 4> points = {Point_2(0, 0), Point_2(10, 0),
                                     Point_2(10, 10), Point_2(0, 10)};
    for (size_t i = 0; i < points.size(); ++i) {
      CGAL::insert(arrangement,
                   Segment_2(points[i], points[(i + 1) % points.size()]));
    }
    CGAL::insert(arrangement, Segment_2(Point_2(5, 0), Point_2(5, 10)));
    for (auto face : arrangement.face_handles()) {
      if (face->is_unbounded()) continue;
      face->data().in_footprint = true;
    }
    return arrangement;
  }

  double face_centre_x(roofer::Arrangement_2::Face_handle face) {
    double sum = 0;
    size_t count = 0;
    auto edge = face->outer_ccb();
    auto first = edge;
    do {
      sum += CGAL::to_double(edge->source()->point().x());
      ++count;
      edge = edge->next();
    } while (edge != first);
    return sum / double(count);
  }

  // Only the west half of the roof was scanned, on the steep plane
  // z = 2x - 5 (so z rises from -5 at x = 0 to 5 at x = 5). East of x = 5
  // there is no data at all.
  roofer::RasterTools::Raster west_only_heightfield() {
    roofer::RasterTools::Raster raster(1, -1, 11, -1, 11);
    raster.prefill_arrays(roofer::RasterTools::MAX);
    for (size_t col = 0; col < raster.dimx_; ++col) {
      for (size_t row = 0; row < raster.dimy_; ++row) {
        const auto p = raster.getPointFromRasterCoords(col, row);
        if (p[0] > 0 && p[0] < 5 && p[1] > 0 && p[1] < 10) {
          raster.set_val(col, row, 2.0 * p[0] - 5.0);
        }
      }
    }
    return raster;
  }

  roofer::IndexedPlanesWithPoints two_planes() {
    roofer::IndexedPlanesWithPoints planes;
    planes[1] = {roofer::Plane(0, 0, 1, -4), {}};   // flat, z = 4
    planes[2] = {roofer::Plane(2, 0, -1, -5), {}};  // steep, z = 2x - 5
    return planes;
  }

  std::pair<int, int> optimise(float nodata_ceiling_weight) {
    auto arrangement = split_square();
    auto heightfield = west_only_heightfield();
    auto roof_planes = two_planes();
    roofer::IndexedPlanesWithPoints ground_planes;
    auto optimiser = roofer::reconstruction::createArrangementOptimiser();
    optimiser->compute(arrangement, heightfield, roof_planes, ground_planes,
                       {.use_ground = false,
                        .nodata_ceiling_weight = nodata_ceiling_weight,
                        .nodata_ceiling_margin = 1.0F});
    int west = 0, east = 0;
    for (auto face : arrangement.face_handles()) {
      if (face->is_unbounded() || !face->data().in_footprint) continue;
      if (face_centre_x(face) < 5) {
        west = face->data().segid;
      } else {
        east = face->data().segid;
      }
    }
    return {west, east};
  }
}  // namespace

TEST_CASE("the scanned half of the roof gets the plane that fits its data") {
  auto [west, east] = optimise(0.0F);
  CHECK(west == 2);
}

TEST_CASE("without the ceiling penalty an unscanned half inherits the steep plane") {
  // The east face has no measured cell, so every label costs the same there
  // and the smoothness term copies the west label: a roof that climbs to 15 m
  // while nothing above 4 m was ever measured (the ramp of 3DBAG/roofer#172).
  auto [west, east] = optimise(0.0F);
  CHECK(east == 2);
}

TEST_CASE("the ceiling penalty keeps an unscanned half below the measured roof") {
  auto [west, east] = optimise(1.0F);
  CHECK(west == 2);
  CHECK(east == 1);
}
