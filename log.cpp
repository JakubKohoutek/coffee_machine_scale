#include <LittleFS.h>
#include <time.h>

#include "log.h"

const char* logFilePath = "/log.txt";
const char* logTempPath = "/log_tmp.txt";
const size_t maxLogFileSize = 1024 * 100; // 100KB max log size
const size_t truncateTarget = maxLogFileSize * 3 / 4; // Truncate to 75% capacity

void initiateLog() {
    if (!LittleFS.begin()) {
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

String getTimestamp() {
    time_t now;
    struct tm timeinfo;
    time(&now);
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

    // Skip oldest bytes to keep truncateTarget worth of data
    size_t skipBytes = fileSize - truncateTarget;
    logFile.seek(skipBytes);

    // Read forward to the next newline to avoid a partial line
    char buf[256];
    size_t bytesRead = logFile.readBytes(buf, sizeof(buf));
    size_t newlineOffset = 0;
    for (size_t i = 0; i < bytesRead; i++) {
        if (buf[i] == '\n') {
            newlineOffset = i + 1;
            break;
        }
    }

    // Position at first complete line after skip
    size_t copyFrom = skipBytes + newlineOffset;
    logFile.seek(copyFrom);

    // Copy remaining data to temp file using fixed buffer (no heap String)
    File tmpFile = LittleFS.open(logTempPath, "w");
    if (!tmpFile) {
        logFile.close();
        Serial.println("[LOG] Failed to create temp file for truncation");
        return;
    }

    tmpFile.println("--- Older entries removed due to size limit ---");

    while (logFile.available()) {
        size_t n = logFile.readBytes(buf, sizeof(buf));
        tmpFile.write((uint8_t*)buf, n);
    }

    logFile.close();
    tmpFile.close();

    // Replace original with truncated version
    LittleFS.remove(logFilePath);
    LittleFS.rename(logTempPath, logFilePath);
}

void logMessage(const String& message) {
    String timestampedMessage = "[" + getTimestamp() + "] " + message;

    if (Serial) {
        Serial.println(timestampedMessage.c_str());
    }

    truncateLogIfNeeded();

    File logFile = LittleFS.open(logFilePath, "a");
    if (!logFile) {
        Serial.println("[LOG] Failed to open log file");
        return;
    }
    logFile.println(timestampedMessage);
    logFile.close();
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
