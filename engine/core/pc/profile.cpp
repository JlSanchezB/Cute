#include <core/profile.h>
#include <display/display.h>

//Profiles enabled
#define USE_MICROPROFILE
#define USE_PIX_PROFILER

#ifdef USE_MICROPROFILE
	#define MICROPROFILE_PER_THREAD_BUFFER_SIZE ((8 * 2048)<<10)
	#define MICROPROFILE_MAX_FRAME_HISTORY ((8 * 2)<<10)
	#define MICROPROFILE_IMPL
	#include <ext/microprofile/microprofile.h>
#endif

#ifdef USE_PIX_PROFILER
	#define USE_PIX
	#include <windows.h>
	#include <d3d12.h>
	#include <pix3.h>
//Display is going to define this function, so we can extract the commandlist from a display context

namespace display
{
	ID3D12GraphicsCommandList* GetCommandListFromDisplayContext(display::Context* context);
}
#endif

#if PROFILE_ENABLE == 1

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

	ProfileScopeGPU::ProfileScopeGPU(ProfileMarker& marker, display::Context* context) : m_marker(marker), m_context(context)
	{
#ifdef USE_MICROPROFILE
		m_data = MicroProfileEnter(m_marker.m_data);
#endif

#ifdef USE_PIX_PROFILER
		PIXBeginEvent(GetCommandListFromDisplayContext(m_context), m_marker.m_colour, m_marker.m_name);
#endif
	}
	ProfileScopeGPU::~ProfileScopeGPU()
	{
#ifdef USE_MICROPROFILE
		MicroProfileLeave(m_marker.m_data, m_data);
#endif

#ifdef USE_PIX_PROFILER
		PIXEndEvent(GetCommandListFromDisplayContext(m_context));
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
	void OnThreadCreate(const char* thread_name)
	{
		MicroProfileOnThreadCreate(thread_name);
	}
}
#endif