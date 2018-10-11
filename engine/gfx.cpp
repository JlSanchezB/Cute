#include "gfx.h"

namespace
{
	// Error and assert hooks
	void(*error_hook)(const char*) = nullptr;
	void(*assert_hook)(const char*) = nullptr;

	void GfxError(const char* msg)
	{
		if (error_hook)
		{
			error_hook(msg);
		}
	}

	void GfxAssert(const char* msg)
	{
		if (assert_hook)
		{
			assert_hook(msg);
		}
	}

	//Singleton
	struct GfxManager
	{

	};

	GfxManager* gfx_manager;
}

namespace gfx
{
	void RegisterErrorHook(void(*hook)(const char*))
	{

	}

	void RegisterAssertHook(void(*hook)(const char*))
	{

	}
}