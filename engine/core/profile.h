//////////////////////////////////////////////////////////////////////////
// Cute engine - Common interface for profile in cute, can connect to few profilers
//////////////////////////////////////////////////////////////////////////

#ifndef PROFILE_H_
#define PROFILE_H_

#define PROFILE_ENABLE

#ifdef PROFILE_ENABLE

#include <stdint.h>
namespace core
{
	//Defines a marker for profiling
	class ProfileMarker
	{
	public:
		ProfileMarker(const char* group, const char* name, const char* full_name, uint32_t colour);

	private:
		uint64_t m_data;

		friend class ProfileScope;
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

	//Init profiles
	void InitProfiler();
	//Shutdown profiles
	void ShutdownProfiler();
	//Flip profiles
	void FlipProfiler();
}

#define PASTE_HELPER(a,b) a ## b
#define PASTE(a,b) PASTE_HELPER(a,b)

#define PROFILE_DEFINE_MARKER(var, group, name, colour) inline core::ProfileMarker var(group, name, group ## name, colour);
#define PROFILE_SCOPE(group, name, colour) static core::ProfileMarker PASTE(g_profile_marker_,__LINE__)(group, name, group ## name, colour); core::ProfileScope PASTE(g_profile_scope_,__LINE__)(PASTE(g_profile_marker_,__LINE__));
#define PROFILE_SCOPE_MARKER(marker) core::ProfileScope PASTE(g_profile_scope_, __LINE__)(marker);

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

#define PROFILE_SCOPE(group, name, colour)
#define PROFILE_SCOPEM(marker)

#endif

#endif //PROFILE_H_
