#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

extern const char* logFilePath;

String        getTimestamp();

void          initiateLog  ();

void          truncateLogIfNeeded ();

void          logMessage (const String& message);

String        readLog();

#endif // LOG_H
