// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "py_sessionlog.h"

#include <chiaki/log.h>
// #include <boost/filesystem.hpp>

static void LogCb(ChiakiLogLevel level, const char *msg, void *user);

SessionLog::SessionLog(StreamSession *session, uint32_t level_mask, const std::string &filename)
	: session(session)
{
    chiaki_log_init(&log, level_mask, LogCb, this);

    if (filename.empty())
    {
        CHIAKI_LOGI(&log, "Logging to file disabled");
    }
    else
    {
        //file->open(filename, std::ios::out | std::ios::app);
        /*if (!file->is_open())
        {
            CHIAKI_LOGI(&log, "Failed to open file %s for logging", filename.c_str());
        }
        else
        {
            CHIAKI_LOGI(&log, "Logging to file %s", filename.c_str());
        }*/
    }

    CHIAKI_LOGI(&log, "Chiaki-Py Version " CHIAKI_PY_VERSION);
}

SessionLog::~SessionLog()
{
	// delete file;
}

void SessionLog::Log(ChiakiLogLevel level, const char *msg)
{
	chiaki_log_cb_print(level, msg, nullptr);

    /*if (file->is_open())
    {
        std::lock_guard<std::mutex> lock(file_mutex);
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream timestamp;
        timestamp << std::put_time(std::localtime(&now_time), "[%Y-%m-%d %H:%M:%S.")
                  << std::setw(3) << std::setfill('0') << ms.count() << "] ";

        (*file) << timestamp.str()
             << "[" << chiaki_log_level_char(level) << "] "
             << msg << std::endl;
        file->flush();
    }*/
}

class SessionLogPrivate
{
	public:
		static void Log(SessionLog *log, ChiakiLogLevel level, const char *msg) { log->Log(level, msg); }
};

static void LogCb(ChiakiLogLevel level, const char *msg, void *user)
{
	auto log = reinterpret_cast<SessionLog *>(user);
	SessionLogPrivate::Log(log, level, msg);
}

#define KEEP_LOG_FILES_COUNT 5

std::string GetLogBaseDir()
{
    /*std::string base_dir = boost::filesystem::temp_directory_path().string();
    std::string log_dir = base_dir + "/chiaki_logs";
    boost::filesystem::create_directories(log_dir);*/
    return "/chiaki_logs";
}

std::string CreateLogFilename()
{
    const std::string date_format = "%Y-%m-%d_%H-%M-%S";
    std::string dir_str = GetLogBaseDir();
    if (dir_str.empty())
        return "";

    std::ostringstream filename;
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);

    filename << "chiaki_session_" << std::put_time(std::localtime(&now_time), date_format.c_str()) << ".log";
    std::string log_file = dir_str + "/" + filename.str();

    // Cleanup old logs
    /*std::vector<boost::filesystem::directory_entry> log_files;
    for (const auto &entry : boost::filesystem::directory_iterator(dir_str))
    {
        if (entry.path().extension() == ".log")
            log_files.push_back(entry);
    }

    std::sort(log_files.begin(), log_files.end(),
              [](const boost::filesystem::directory_entry &a, const boost::filesystem::directory_entry &b)
              { return boost::filesystem::last_write_time(a) > boost::filesystem::last_write_time(b); });

    while (log_files.size() > KEEP_LOG_FILES_COUNT)
    {
        boost::filesystem::remove(log_files.back().path());
        log_files.pop_back();
    }*/

    return log_file;
}
