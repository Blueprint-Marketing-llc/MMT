#ifndef UTIL_USAGE_H
#define UTIL_USAGE_H
#include <cstddef>
#include <iosfwd>
#include <string>
#include <stdint.h>

namespace mmt {
    namespace sapt {
// Time in seconds since process started.  Zero on unsupported platforms.
        double WallTime();

    } // namespace sapt
} // namespace mmt
#endif // UTIL_USAGE_H
