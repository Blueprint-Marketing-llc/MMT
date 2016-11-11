#include <iostream>
#include <iomanip>
#include "util/usage.hh"
#include "Timer.h"

using namespace std;

namespace mmt {
    namespace sapt {

//global variable
        Timer g_timer;


        void ResetUserTime() {
          g_timer.start();
        };

        void PrintUserTime(const std::string &message) {
          g_timer.check(message.c_str());
        }

        double GetUserTime() {
          return g_timer.get_elapsed_time();
        }


/***
 * Return the total wall time that the timer has been in the "running"
 * state since it was first "started".
 */
        double Timer::get_elapsed_time() const {
          if (stopped) {
            return stop_time - start_time;
          }
          if (running) {
            return mmt::sapt::WallTime() - start_time;
          }
          return 0;
        }

/***
 * Reset a timer.  If it is already running, stop and reset to 0
 * Print an optional message.
 */
        void Timer::reset(const char* msg) {
            // Print an optional message, something like "Starting timer t";
            if (msg) cerr << msg << endl;

            start_time = 0;
            running = false;
            stopped = false;
            start_time = 0;
        }

/***
 * Start a timer.  If it is already running, let it continue running.
 * Print an optional message.
 */
        void Timer::start(const char *msg) {
          // Print an optional message, something like "Starting timer t";
          if (msg) cerr << msg << endl;

          // Return immediately if the timer is already running
          if (running && !stopped) return;

          // If stopped, recompute start time
          if (stopped) {
            start_time = mmt::sapt::WallTime() - (stop_time - start_time);
            stopped = false;
          } else {
            start_time = mmt::sapt::WallTime();
            running = true;
          }
        }

/***
 * Stop a timer.
 * Print an optional message.
 */
        void Timer::stop(const char *msg) {
          // Print an optional message, something like "Stopping timer t";
          if (msg) cerr << msg << endl;

          // Return immediately if the timer is not running
          if (stopped || !running) return;

          // Record stopped time
          stop_time = mmt::sapt::WallTime();

          // Change timer status to running
          stopped = true;
        }

/***
 * Print out an optional message followed by the current timer timing.
 */
        void Timer::check(const char *msg) {
          // Print an optional message, something like "Checking timer t";
          if (msg) cerr << msg << " : ";

          cerr << "[" << (running ? get_elapsed_time() : 0) << "] seconds" << endl;
        }

/***
 * Allow timers to be printed to ostreams using the syntax 'os << t'
 * for an ostream 'os' and a timer 't'.  For example, "cout << t" will
 * print out the total amount of time 't' has been "running".
 */
        std::ostream &operator<<(std::ostream &os, Timer &t) {
          os << (t.running ? t.get_elapsed_time() : 0);
          return os;
        }

    }
}

