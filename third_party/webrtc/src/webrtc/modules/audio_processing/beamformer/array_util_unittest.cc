/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/beamformer/array_util.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

TEST(ArrayUtilTest, GetMinimumSpacing) {
  std::vector<Point> array_geometry;
  array_geometry.push_back(Point(0.f, 0.f, 0.f));
  array_geometry.push_back(Point(0.1f, 0.f, 0.f));
  EXPECT_FLOAT_EQ(0.1f, GetMinimumSpacing(array_geometry));
  array_geometry.push_back(Point(0.f, 0.05f, 0.f));
  EXPECT_FLOAT_EQ(0.05f, GetMinimumSpacing(array_geometry));
  array_geometry.push_back(Point(0.f, 0.f, 0.02f));
  EXPECT_FLOAT_EQ(0.02f, GetMinimumSpacing(array_geometry));
  array_geometry.push_back(Point(-0.003f, -0.004f, 0.02f));
  EXPECT_FLOAT_EQ(0.005f, GetMinimumSpacing(array_geometry));
}

}  // namespace webrtc
