//
// Created by david on 25.11.16.
//

#ifndef MMT_LOGGING_LOGGER_H
#define MMT_LOGGING_LOGGER_H

#include <iostream>
#include <sstream>
#include <climits>
//#include <boost/thread/mutex.hpp>
//#include <boost/thread/locks.hpp>

namespace mmt {
namespace logging {

class LogLock {
public:
  //LogLock() : m_lock(s_logMutex) {}
  LogLock() {}

  ~LogLock() {}

  static int verboseLevel() {
    return s_verboseLevel;
  }

  static void SetVerboseLevel(int level) {
    s_verboseLevel = level;
  }

  void LogMessage(const std::string &message) {
    std::cerr << message << std::endl;
  }

private:
  //boost::lock_guard <boost::mutex> m_lock;
  //static boost::mutex s_logMutex;
  static int s_verboseLevel;
};

/**
 * ordered in a way that numeric values can be compared with operator<(),
 * higher priority means more important (reverse ordering compared to old moses1 loglevels!)
 */
enum LogLevel {
  OFF = INT_MAX,
  TRACE = 500,
  DEBUG = 1000,
  INFO = 2000,
  WARN = 3000,
  ERROR = 4000,
  FATAL = 5000,
  ALL = 0
};

#define IsLogLevelEnabled(prio) (mmt::logging::LogLock::verboseLevel() <= mmt::logging::prio)

#define Log(prio, msg) \
  do { \
    if(IsLogLevelEnabled(prio)) { \
      std::stringstream ss; \
      ss << msg; \
      mmt::logging::LogLock lock; \
      lock.LogMessage(ss.str()); \
    } \
  } while(0)

#define Logd(msg) Log(DEBUG, msg)
#define Logi(msg) Log(INFO, msg)

} // namespace logging
} // namespace mmt

#endif //MMT_LOGGING_LOGGER_H
