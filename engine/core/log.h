//////////////////////////////////////////////////////////////////////////
// Cute engine - Log system
//////////////////////////////////////////////////////////////////////////

#ifndef LOG_H_
#define LOG_H_

#include <string>
#include <iostream>

namespace core
{
	void log(const char* message, ...);
	void log_warning(const char* message, ...);
	void log_error(const char* message, ...);
}
#endif //LOG_H
