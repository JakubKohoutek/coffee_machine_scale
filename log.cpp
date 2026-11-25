#include <LittleFS.h>
#include <time.h>

#include "log.h"

const char* logFilePath = "/log.txt";
const char* previousLogFilePath = "/log_previous.txt";
const size_t maxLogFileSize = 1024 * 100; // 100KB max log size
unsigned long maxLogWriteMs = 0;
static bool logReady = false;

/** Mount persistent diagnostics without formatting a failed filesystem. */
void initiateLog() {
    LittleFSConfig config;
    config.setAutoFormat(false);
    LittleFS.setConfig(config);
    logReady = LittleFS.begin();
    if (!logReady) {
        Serial.println("[LOG] LittleFS mount failed");
        return;
    }
    if (!LittleFS.exists(logFilePath)) {
        File logFile = LittleFS.open(logFilePath, "w");
        if (logFile) {
            logFile.println("[LOG] log initiated!");
            logFile.close();
        }
    }
    else {
        File logFile = LittleFS.open(logFilePath, "a");
        if (logFile) {
            logFile.println("[LOG] new session started!");
            logFile.close();
        }
    }
}

/** Label unsynchronized time explicitly; every log entry also includes uptime. */
String getTimestamp() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    if (now < 1609459200) return "clock-unsynced";
    localtime_r(&now, &timeinfo);
    unsigned long millisecond = millis() % 1000;
    char buffer[30];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d.%02d.%04d %02d:%02d:%02d.%03lu",
        timeinfo.tm_mday,
        timeinfo.tm_mon + 1,
        timeinfo.tm_year + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        millisecond
    );

    return String(buffer);
}

/** Rotate by rename instead of copying 75KB in the input-processing thread. */
void truncateLogIfNeeded() {
    File logFile = LittleFS.open(logFilePath, "r");
    if (!logFile) {
        Serial.println("[LOG] Failed to open log file for truncation check");
        return;
    }

    size_t fileSize = logFile.size();
    if (fileSize <= maxLogFileSize) {
        logFile.close();
        return;
    }

    logFile.close();
    if (LittleFS.exists(previousLogFilePath)) LittleFS.remove(previousLogFilePath);
    if (!LittleFS.rename(logFilePath, previousLogFilePath)) {
        Serial.println("[LOG] Rotation failed; keeping current log");
    }
}

/** Persist uptime with wall time and measure storage latency without recursive logging. */
void logMessage(const String& message) {
    unsigned long started = millis();
    String timestampedMessage;
    timestampedMessage.reserve(message.length() + 64);
    timestampedMessage += "[";
    timestampedMessage += getTimestamp();
    timestampedMessage += "][up_ms=";
    timestampedMessage += started;
    timestampedMessage += "] ";
    timestampedMessage += message;

    if (Serial) {
        Serial.println(timestampedMessage.c_str());
    }

    if (!logReady) return;
    truncateLogIfNeeded();

    File logFile = LittleFS.open(logFilePath, "a");
    if (!logFile) {
        Serial.println("[LOG] Failed to open log file");
        return;
    }
    logFile.println(timestampedMessage);
    logFile.close();
    maxLogWriteMs = max(maxLogWriteMs, millis() - started);
}

String readLog() {
    File logFile = LittleFS.open(logFilePath, "r");
    if (!logFile) {
        return "[LOG] Failed to open log file";
    }

    String logContent = logFile.readString();
    logFile.close();
    return logContent;
}
