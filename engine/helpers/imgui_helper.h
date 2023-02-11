#ifndef IMGUI_HELPER_H_
#define IMGUI_HELPER_H_

#include <cstdio>

namespace helpers
{
	inline void FormatMemory(char* buffer, const size_t size, const size_t value)
	{
		if (value < 1024u)
		{
			snprintf(buffer, size, "%zub", value);
		}
		else if (value < 1024u * 1024u)
		{
			snprintf(buffer, size, "%.2fkb", static_cast<double>(value)/1024.0);
		}
		else if (value < 1024u * 1024u * 1024u)
		{
			snprintf(buffer, size, "%.2fmb", static_cast<double>(value) /(1024.0 * 1024.0));
		}
		else if (value < 1024u * 1024u * 1024u)
		{
			snprintf(buffer, size, "%.2fgb", static_cast<double>(value) / (1024.0 * 1024.0 * 1024.0));
		}
		else if (value < 1024u * 1024u * 1024u * 1024u)
		{
			snprintf(buffer, size, "%.2ftb", static_cast<double>(value) / (1024.0 * 1024.0 * 1024.0 * 1024.f));
		}
		else
		{
			snprintf(buffer, size, "INVALID");
		}
	}
}

#endif //IMGUI_HELPER_H_