#include "util/usage.hh"

#include <fstream>
#include <ostream>
#include <sstream>
#include <set>
#include <string>
#include <cstring>
#include <cctype>
#include <ctime>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__MACH__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace mmt {
    namespace ilm {
        namespace {
#if defined(__MACH__)
            typedef struct timeval Wall;
            Wall GetWall() {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return tv;
            }
#elif defined(_WIN32) || defined(_WIN64)
            typedef time_t Wall;
Wall GetWall() {
  return time(NULL);
}
#else
typedef struct timespec Wall;
Wall GetWall() {
  Wall ret;
  clock_gettime(CLOCK_MONOTONIC, &ret);
  return ret;
}
#endif

// gcc possible-unused function flags
#ifdef __GNUC__

            double Subtract(time_t first, time_t second) __attribute__ ((unused));

            double DoubleSec(time_t tv) __attribute__ ((unused));

#if !defined(_WIN32) && !defined(_WIN64)

            double Subtract(const struct timeval &first, const struct timeval &second) __attribute__ ((unused));

            double Subtract(const struct timespec &first, const struct timespec &second) __attribute__ ((unused));

            double DoubleSec(const struct timeval &tv) __attribute__ ((unused));

            double DoubleSec(const struct timespec &tv) __attribute__ ((unused));

#endif
#endif

// Some of these functions are only used on some platforms.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

// These all assume first > second
            double Subtract(time_t first, time_t second) {
                return difftime(first, second);
            }

            double DoubleSec(time_t tv) {
                return static_cast<double>(tv);
            }

#if !defined(_WIN32) && !defined(_WIN64)

            double Subtract(const struct timeval &first, const struct timeval &second) {
                return static_cast<double>(first.tv_sec - second.tv_sec) +
                       static_cast<double>(first.tv_usec - second.tv_usec) / 1000000.0;
            }

            double Subtract(const struct timespec &first, const struct timespec &second) {
                return static_cast<double>(first.tv_sec - second.tv_sec) +
                       static_cast<double>(first.tv_nsec - second.tv_nsec) / 1000000000.0;
            }

            double DoubleSec(const struct timeval &tv) {
                return static_cast<double>(tv.tv_sec) + (static_cast<double>(tv.tv_usec) / 1000000.0);
            }

            double DoubleSec(const struct timespec &tv) {
                return static_cast<double>(tv.tv_sec) + (static_cast<double>(tv.tv_nsec) / 1000000000.0);
            }

#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif

            class RecordStart {
            public:
                RecordStart() {
                    started_ = GetWall();
                }

                const Wall &Started() const {
                    return started_;
                }

            private:
                Wall started_;
            };

            const RecordStart kRecordStart;
        } // namespace

        double WallTime() {
            return Subtract(GetWall(), kRecordStart.Started());
        }


    } // namespace ilm
} // namespace mmt
