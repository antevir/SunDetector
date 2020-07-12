#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>
#include "settings.h"

#define SYSLOG_PORT 514

#define SYSLOG_HOST HOSTNAME ".local"
#define SYSLOG_APP HOSTNAME
#define MAX_PACKET_SIZE 500

#define PRI_DEBUG 15   // 8 + 7
#define PRI_INFO 14    // 8 + 6
#define PRI_WARNING 12 // 8 + 4
#define PRI_ERROR 11   // 8 + 3

class LogImpl
{
public:
    LogImpl()
    {
    }

    void begin()
    {
#ifdef LOG_USE_SERIAL
        Serial.begin(LOG_SERIAL_BAUDRATE);
        Serial.println("");
#endif
    }
    void error(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        write(PriError, fmt, args);
        va_end(args);
    }
    void warn(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        write(PriWarning, fmt, args);
        va_end(args);
    }
    void info(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        write(PriInfo, fmt, args);
        va_end(args);
    }

protected:
    enum Prio
    {
        PriInfo = PRI_INFO,
        PriWarning = PRI_WARNING,
        PriError = PRI_ERROR
    };

    WiFiUDP udp;

    void write(Prio pri, const char *fmt, va_list args)
    {
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), fmt, args);

#ifdef LOG_USE_SYSLOG
        writeSysLog(pri, buffer);
#endif
#ifdef LOG_USE_SERIAL
        writeSerial(pri, buffer);
#endif
    }

    void writeSysLog(Prio pri, char *message)
    {
        char buffer[64];
        snprintf((char *)buffer, sizeof(buffer), "<%d> %s %s: ", pri, SYSLOG_HOST, SYSLOG_APP);
        udp.beginPacket(LOG_SYSLOG_SERVER, SYSLOG_PORT);
        udp.write(buffer, strlen(buffer));
        udp.write(message, strlen(message));
        udp.endPacket();
    }

    const char *priToString(Prio pri)
    {
        switch (pri)
        {
        case PriInfo:
            return "Info";
        case PriWarning:
            return "Warning";
        case PriError:
            return "Error";
        default:
            return "";
        }
    }

    void writeSerial(Prio pri, char *message)
    {
        Serial.print("<");
        Serial.print(priToString(pri));
        Serial.print("> ");
        Serial.println(message);
    }
};

extern LogImpl Log;