//
// Created by david on 25.11.16.
//

#include "Logger.h"

namespace Moses2 {

boost::mutex LogLock::s_logMutex;
int LogLock::s_verboseLevel = 0;

} // namespace Moses2
