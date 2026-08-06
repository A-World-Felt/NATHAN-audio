#include "spatial_math.h"

#include <algorithm>
#include <cmath>

namespace web_demo {

float distanceGain(float distanceMeters, float referenceDistance, float maxDistance,
                    float rolloffFactor) {
    float d = std::clamp(distanceMeters, referenceDistance, maxDistance);
    return referenceDistance / (referenceDistance + rolloffFactor * (d - referenceDistance));
}

float distanceMeters(float relX, float relZ) {
    return std::sqrt(relX * relX + relZ * relZ);
}

float azimuthDegrees(float relX, float relZ) {
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return std::atan2(relX, -relZ) * kRadToDeg;
}

}  // namespace web_demo
