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

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"

namespace webrtc {

float GetMinimumSpacing(const std::vector<Point>& array_geometry) {
  RTC_CHECK_GT(array_geometry.size(), 1u);
  float mic_spacing = std::numeric_limits<float>::max();
  for (size_t i = 0; i < (array_geometry.size() - 1); ++i) {
    for (size_t j = i + 1; j < array_geometry.size(); ++j) {
      mic_spacing =
          std::min(mic_spacing, Distance(array_geometry[i], array_geometry[j]));
    }
  }
  return mic_spacing;
}

}  // namespace webrtc
