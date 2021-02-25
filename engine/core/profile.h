//////////////////////////////////////////////////////////////////////////
// Cute engine - Common interface for profile in cute, can connect to few profilers
//////////////////////////////////////////////////////////////////////////

#ifndef PROFILE_H_
#define PROFILE_H_

#ifndef PROFILE_ENABLE
	#define PROFILE_ENABLE 1
#endif

#if PROFILE_ENABLE == 1

#include <stdint.h>

namespace display
{
	struct Context;
}

namespace core
{
	//Defines a marker for profiling
	class ProfileMarker
	{
	public:
		ProfileMarker(const char* group, const char* name, const char* full_name, uint32_t colour);

	private:

		//Data for miniprofiler
		uint64_t m_data;

		//Data for pix
		const char* m_name;
		uint32_t m_colour;

		friend class ProfileScope;
		friend class ProfileScopeGPU;
	};

	//Profile the marker
	class ProfileScope
	{
	public:
		ProfileScope(ProfileMarker& marker);
		~ProfileScope();

	private:
		uint64_t m_data;
		ProfileMarker& m_marker;
	};

	//Profile the marker
	class ProfileScopeGPU
	{
	public:
		ProfileScopeGPU(ProfileMarker& marker, display::Context* context);
		~ProfileScopeGPU();

	private:
		uint64_t m_data;
		ProfileMarker& m_marker;
		display::Context* m_context;
	};

	//Init profiles
	void InitProfiler();
	//Shutdown profiles
	void ShutdownProfiler();
	//Flip profiles
	void FlipProfiler();
	//Set the thread name
	void OnThreadCreate(const char* thread_name);
}

#define PASTE_HELPER(a,b) a ## b
#define PASTE(a,b) PASTE_HELPER(a,b)

#define PROFILE_DEFINE_MARKER(var, group, colour, name) inline core::ProfileMarker var(group, name, group ## "-" ##  name, colour);
#define PROFILE_SCOPE(group, colour, name) static core::ProfileMarker PASTE(g_profile_marker_,__LINE__)(group, name, group ## "-" ## name, colour); core::ProfileScope PASTE(g_profile_scope_,__LINE__)(PASTE(g_profile_marker_,__LINE__));
#define PROFILE_SCOPE_MARKER(marker) core::ProfileScope PASTE(g_profile_scope_, __LINE__)(marker);
#define PROFILE_SCOPE_GPU(context, group, colour, name) static core::ProfileMarker PASTE(g_profile_marker_,__LINE__)(group, name, group ## "-" ## name, colour); core::ProfileScopeGPU PASTE(g_profile_scope_,__LINE__)(PASTE(g_profile_marker_,__LINE__), context);

#define PROFILE_SCOPE_ARG(group, colour, name,...)	char PASTE(profile_buffer_,__LINE__)[256]; \
													sprintf_s(PASTE(profile_buffer_, __LINE__), 256, group ## "-" ## name, __VA_ARGS__);\
													core::ProfileMarker PASTE(g_profile_marker_,__LINE__)(group, PASTE(profile_buffer_,__LINE__)+ strlen(group) + 1, PASTE(profile_buffer_,__LINE__), colour); core::ProfileScope PASTE(g_profile_scope_,__LINE__)(PASTE(g_profile_marker_,__LINE__));
#define PROFILE_SCOPE_GPU_ARG(context, group, colour, name,...)	char PASTE(profile_buffer_,__LINE__)[256]; \
																sprintf_s(PASTE(profile_buffer_, __LINE__), 256, group ## "-" ## name, __VA_ARGS__);\
																core::ProfileMarker PASTE(g_profile_marker_,__LINE__)(group, PASTE(profile_buffer_,__LINE__) + strlen(group) + 1, PASTE(profile_buffer_,__LINE__), colour); core::ProfileScopeGPU PASTE(g_profile_scope_,__LINE__)(PASTE(g_profile_marker_,__LINE__), context);
#else
namespace core
{
	class ProfileMarker
	{
	};
	class ProfileScope
	{
	};
}

#define PROFILE_DEFINE_MARKER(var, group, colour, name)
#define PROFILE_SCOPE(group, colour, name)
#define PROFILE_SCOPE(marker)
#define PROFILE_SCOPE_GPU(context, group, colour, name)
#define PROFILE_SCOPE_ARG(group, colour, name,...)
#define PROFILE_SCOPE_GPU_ARG(context, group, colour, name,...)

#endif

#endif //PROFILE_H_
