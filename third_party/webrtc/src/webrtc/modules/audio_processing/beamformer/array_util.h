/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_

#include <cmath>
#include <vector>

namespace webrtc {

// Coordinates in meters.
template<typename T>
struct CartesianPoint {
  CartesianPoint(T x, T y, T z) {
    c[0] = x;
    c[1] = y;
    c[2] = z;
  }
  T x() const { return c[0]; }
  T y() const { return c[1]; }
  T z() const { return c[2]; }
  T c[3];
};

using Point = CartesianPoint<float>;

// Returns the minimum distance between any two Points in the given
// |array_geometry|.
float GetMinimumSpacing(const std::vector<Point>& array_geometry);

template<typename T>
float Distance(CartesianPoint<T> a, CartesianPoint<T> b) {
  return std::sqrt((a.x() - b.x()) * (a.x() - b.x()) +
                   (a.y() - b.y()) * (a.y() - b.y()) +
                   (a.z() - b.z()) * (a.z() - b.z()));
}

template <typename T>
struct SphericalPoint {
  SphericalPoint(T azimuth, T elevation, T radius) {
    s[0] = azimuth;
    s[1] = elevation;
    s[2] = radius;
  }
  T azimuth() const { return s[0]; }
  T elevation() const { return s[1]; }
  T distance() const { return s[2]; }
  T s[3];
};

using SphericalPointf = SphericalPoint<float>;

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_
