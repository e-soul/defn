// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BACKGROUND_LAYER_H
#define BACKGROUND_LAYER_H

#include <string>

namespace defn {

/// One plane of a parallax background.
///
/// A level either names a single `background` image, which is drawn edge to edge and scrolls with the camera,
/// or a list of these, which are drawn back to front and each scroll at their own rate. The two are not
/// variations of one another: a single image paints its own depth and then contradicts it by moving rigidly,
/// while a stack expresses depth through the motion itself.
///
/// Every measurement is a fraction of viewport height rather than a pixel count, so the artwork's own
/// resolution is free to differ per layer -- a sky with no hard edges is stored at half the density of the
/// combat floor, and neither the data nor the builder has to know.
struct BackgroundLayer {
    std::string path;

    /// Camera scroll multiplier. Smaller is further away; 1.0 moves with the world.
    ///
    /// The floor a level is fought on must stay at 1.0. Units live in its coordinate space, and a floor that
    /// slides under the sprites standing on it reads as ice rather than as depth.
    float scroll_scale = 1.0F;

    /// Drawn height, as a fraction of viewport height. The layer's width follows from its aspect ratio, which
    /// is why layers naturally end up different widths -- and unequal widths are what stop the whole stack
    /// repeating on one period.
    float height_ratio = 1.0F;

    /// Where the layer's bottom edge sits, as a fraction of viewport height from the top. The ground's far
    /// edge sits at 0.5, so a band standing on it has a `bottom_ratio` of 0.5 and a band further away than it
    /// has a smaller one.
    float bottom_ratio = 1.0F;

    /// Horizontal drift in pixels per second, independent of the camera. Zero for everything that is part of
    /// the world: a layer that moves on its own has left the ground plane, so this is only for weather and
    /// sky. Clouds are the case it exists for -- a drifting cloud plane is the cheapest motion in the stack,
    /// because it keeps going while the camera stands still, which is most of a match.
    float autoscroll = 0.0F;
};

} // namespace defn

#endif
