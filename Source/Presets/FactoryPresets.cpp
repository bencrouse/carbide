#include "FactoryPresets.h"

namespace FactoryPresets
{
std::vector<Preset> getPresets()
{
    return {
        // Core
        { "Core / Needle Punch",  { 0.55f, 0.18f, 0.84f, 0.62f, 0.22f, 0.32f, 0.54f, 0.34f, 0.68f }, 1 },
        { "Core / Short Alloy",   { 0.50f, 0.24f, 0.74f, 0.56f, 0.30f, 0.36f, 0.50f, 0.42f, 0.69f }, 1 },
        { "Core / Dry Mono Rail", { 0.46f, 0.17f, 0.88f, 0.40f, 0.24f, 0.24f, 0.46f, 0.30f, 0.70f }, 1 },
        { "Core / Arc Core",      { 0.52f, 0.27f, 0.70f, 0.52f, 0.40f, 0.50f, 0.58f, 0.50f, 0.66f }, 1 },
        { "Core / Pulse Stub",    { 0.49f, 0.21f, 0.82f, 0.54f, 0.34f, 0.34f, 0.52f, 0.44f, 0.67f }, 1 },

        // Click
        { "Click / Glass Foot",   { 0.50f, 0.14f, 0.93f, 0.84f, 0.50f, 0.40f, 0.72f, 0.20f, 0.63f }, 2 },
        { "Click / Tin Snap",     { 0.60f, 0.14f, 0.94f, 0.92f, 0.66f, 0.45f, 0.78f, 0.15f, 0.59f }, 2 },
        { "Click / Laser Tap",    { 0.66f, 0.10f, 0.99f, 0.98f, 0.74f, 0.52f, 0.84f, 0.10f, 0.55f }, 2 },
        { "Click / Punch Film",   { 0.57f, 0.13f, 0.90f, 0.76f, 0.28f, 0.28f, 0.62f, 0.24f, 0.65f }, 2 },
        { "Click / Air Needle",   { 0.62f, 0.12f, 0.95f, 0.88f, 0.42f, 0.30f, 0.80f, 0.16f, 0.60f }, 2 },

        // Weight
        { "Weight / Needle Sub",    { 0.40f, 0.26f, 0.82f, 0.56f, 0.18f, 0.28f, 0.42f, 0.76f, 0.70f }, 0 },
        { "Weight / Future Brick",  { 0.51f, 0.34f, 0.68f, 0.44f, 0.52f, 0.60f, 0.44f, 0.68f, 0.63f }, 1 },
        { "Weight / Silk Hammer",   { 0.44f, 0.36f, 0.66f, 0.32f, 0.16f, 0.18f, 0.34f, 0.84f, 0.72f }, 0 },
        { "Weight / Hollow Carbon", { 0.41f, 0.40f, 0.60f, 0.34f, 0.46f, 0.56f, 0.30f, 0.78f, 0.62f }, 0 },
        { "Weight / Bedrock",       { 0.39f, 0.43f, 0.62f, 0.30f, 0.24f, 0.30f, 0.34f, 0.88f, 0.68f }, 0 },

        // Edge
        { "Edge / Crushed Rubber", { 0.44f, 0.24f, 0.70f, 0.52f, 0.66f, 0.66f, 0.42f, 0.54f, 0.64f }, 2 },
        { "Edge / Grain Impact",   { 0.51f, 0.22f, 0.78f, 0.74f, 0.70f, 0.72f, 0.58f, 0.30f, 0.59f }, 2 },
        { "Edge / Chrome Bite",    { 0.56f, 0.18f, 0.84f, 0.72f, 0.76f, 0.78f, 0.64f, 0.20f, 0.57f }, 2 },
        { "Edge / Iron Flick",     { 0.63f, 0.11f, 0.95f, 0.86f, 0.70f, 0.62f, 0.76f, 0.12f, 0.56f }, 2 },
        { "Edge / Hard Resin",     { 0.47f, 0.20f, 0.80f, 0.56f, 0.74f, 0.70f, 0.48f, 0.44f, 0.60f }, 2 },

        // Soft
        { "Soft / Soft Collapse", { 0.38f, 0.46f, 0.58f, 0.22f, 0.20f, 0.20f, 0.24f, 0.84f, 0.72f }, 0 },
        { "Soft / Velvet Knock",  { 0.42f, 0.38f, 0.64f, 0.20f, 0.14f, 0.16f, 0.28f, 0.78f, 0.74f }, 0 },
        { "Soft / Pillow Tap",    { 0.36f, 0.35f, 0.60f, 0.18f, 0.10f, 0.12f, 0.22f, 0.82f, 0.75f }, 0 },
        { "Soft / Round Bloom",   { 0.43f, 0.41f, 0.62f, 0.24f, 0.18f, 0.18f, 0.26f, 0.84f, 0.73f }, 0 },
        { "Soft / Haze Pulse",    { 0.40f, 0.37f, 0.64f, 0.20f, 0.24f, 0.16f, 0.30f, 0.76f, 0.71f }, 0 }
    };
}
} // namespace FactoryPresets
