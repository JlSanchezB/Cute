#include <core/profile.h>

//Profiles enabled
#define USE_MICROPROFILE
#define USE_PIX_PROFILER

#ifdef USE_MICROPROFILE
	#define MICROPROFILE_MAX_FRAME_HISTORY (2<<10)
	#define MICROPROFILE_IMPL
	#include <ext/microprofile/microprofile.h>
#endif

#ifdef USE_PIX_PROFILER
	#define USE_PIX
	#include <windows.h>
	#include "pix3.h"
#endif

namespace core
{
	ProfileMarker::ProfileMarker(const char* group, const char* name, const char* full_name, uint32_t colour)
	{
#ifdef USE_MICROPROFILE
		m_data = MicroProfileGetToken(group, name, colour, MicroProfileTokenTypeCpu);
#endif

#ifdef USE_PIX_PROFILER
		m_name = full_name;
		m_colour = colour;
#endif
	}

	ProfileScope::ProfileScope(ProfileMarker& marker) : m_marker(marker)
	{
#ifdef USE_MICROPROFILE
		m_data = MicroProfileEnter(m_marker.m_data);
#endif

#ifdef USE_PIX_PROFILER
		PIXBeginEvent(m_marker.m_colour, m_marker.m_name);
#endif
	}
	ProfileScope::~ProfileScope()
	{
#ifdef USE_MICROPROFILE
		MicroProfileLeave(m_marker.m_data, m_data);
#endif

#ifdef USE_PIX_PROFILER
		PIXEndEvent();
#endif
	}

	void InitProfiler()
	{
#ifdef USE_MICROPROFILE
		//Init microprofiler
		MicroProfileOnThreadCreate("Main");
		MicroProfileSetForceEnable(true);
		MicroProfileSetEnableAllGroups(true);
		MicroProfileSetForceMetaCounters(true);

		MicroProfileWebServerStart();
#endif
	}

	void ShutdownProfiler()
	{
#ifdef USE_MICROPROFILE
		MicroProfileWebServerStop();
		MicroProfileShutdown();
#endif
	}

	void FlipProfiler()
	{
#ifdef USE_MICROPROFILE
		MicroProfileFlip();
#endif
	}
}