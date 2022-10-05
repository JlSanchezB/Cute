
#include <core/sync.h>
#include <windows.h>

void core::Thread::Init(const wchar_t* name, ThreadPriority thread_priority)
{
	//Set name
	SetThreadDescription(static_cast<HANDLE>(native_handle()), name);

	if (thread_priority == ThreadPriority::Background)
	{
		//Set priority
		SetThreadPriority(static_cast<HANDLE>(native_handle()), THREAD_PRIORITY_BELOW_NORMAL);
	}
}