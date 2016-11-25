//
// Created by david on 25.11.16.
//

#ifndef MMT_DECODER_LOGGER_H
#define MMT_DECODER_LOGGER_H

#include <iostream>
#include <sstream>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

namespace Moses2 {

/**
 * Locks stderr for multi-threaded logging.
 */
class LogLock {
public:
  LogLock() : m_lock(s_logMutex) {}
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
  boost::lock_guard<boost::mutex> m_lock;
  static boost::mutex s_logMutex;
  static int s_verboseLevel;
};

#define LOG(level, msg) \
  do { \
    int verboseLevel = LogLock::verboseLevel(); \
    \
    if(verboseLevel >= level) { \
      std::stringstream ss; \
      ss << msg; \
      LogLock lock; \
      lock.LogMessage(ss.str()); \
    } \
  } while(0)


} // namespace Moses2

#endif //MMT_DECODER_LOGGER_H
