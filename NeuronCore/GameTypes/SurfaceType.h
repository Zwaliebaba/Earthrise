#pragma once

namespace Neuron
{
  // SurfaceType — landscape coloring style for asteroids and planets.
  // Each type maps to a landscape gradient texture under GameData/Terrain/.
  enum class SurfaceType : uint8_t
  {
    Default,      // LandscapeDefault.dds
    IceCap,       // LandscapeIcecaps.dds
    Containment,  // LandscapeContainment.dds
    Desert,       // LandscapeDesert.dds
    Earth,        // LandscapeEarth.dds

    COUNT         // Sentinel for ENUM_HELPER
  };
  ENUM_HELPER(SurfaceType, Default, Earth)
}
