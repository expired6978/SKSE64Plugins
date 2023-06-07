#include "ILogger.h"

#include <shlobj.h>
#include <filesystem>

void DebugLog::OpenRelative(int folderID, const char* relPath)
{
	char	path[MAX_PATH];

	HRESULT err = SHGetFolderPath(NULL, folderID | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, path);
	if (!SUCCEEDED(err))
	{
		_FATALERROR("SHGetFolderPath %08X failed (result = %08X lasterr = %08X)", folderID, err, GetLastError());
	}
	ASSERT_CODE(SUCCEEDED(err), err);

	strcat_s(path, sizeof(path), relPath);
	std::filesystem::path p(path);
	p.remove_filename();
	std::filesystem::create_directories(p);
	m_logger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("logger", path, true);
	spdlog::set_default_logger(m_logger);
	spdlog::flush_every(std::chrono::seconds(10));
}

void DebugLog::SetLogLevel(LogLevel logLevel)
{
	if(m_logger)
		m_logger->set_level(gSpdLogLevelMap[logLevel]);
}
