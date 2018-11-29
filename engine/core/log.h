//////////////////////////////////////////////////////////////////////////
// Cute engine - Log system
//////////////////////////////////////////////////////////////////////////

#ifndef LOG_H_
#define LOG_H_

#include <string>
#include <iostream>

namespace core
{
	void LogInfo(const char* message, ...);
	void LogWarning(const char* message, ...);
	void LogError(const char* message, ...);
}
#endif //LOG_H
