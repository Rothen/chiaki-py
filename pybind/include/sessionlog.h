#ifndef CHIAKI_PY_SESSIONLOG_H
#define CHIAKI_PY_SESSIONLOG_H

#include <time.h>
#include <ctime>
#include <fstream>
#include <string>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chiaki/log.h>

class ChiakiPySession;

class SessionLog
{
    friend class SessionLogPrivate;

    private:
        // ChiakiPySession *session;
        ChiakiLog log;
        // std::ofstream *file;

        void Log(ChiakiLogLevel level, const char *msg);

    public:
        SessionLog(ChiakiPySession *session, uint32_t level_mask, const std::string &filename);
        ~SessionLog();

        ChiakiLog *GetChiakiLog() { return &log; }
};

std::string GetLogBaseDir();
std::string CreateLogFilename();

#endif // CHIAKI_PY_SESSIONLOG_H