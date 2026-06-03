#include "metric.h"

namespace kerr {
const char* modeName(UniverseMode m) {
    switch (m) {
        case UniverseMode::NORMAL:   return "NORMAL";
        case UniverseMode::BETWEEN:  return "BETWEEN";
        case UniverseMode::INSIDE:   return "INSIDE";
        case UniverseMode::NEGATIVE: return "NEGATIVE";
    }
    return "?";
}
} // namespace kerr
