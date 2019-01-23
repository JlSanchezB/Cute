#include <core/virtual_alloc.h>
#include <Windows.h>
#include <stdexcept>

namespace
{
	size_t g_cached_page_size = 0;
}

namespace core
{
	void * VirtualAlloc(void * ptr, size_t size, AllocFlags flags)
	{
		DWORD allocation_type = 0;
		if (check_flag(flags, AllocFlags::Commit))
		{
			allocation_type |= MEM_COMMIT;
		}
		if (check_flag(flags, AllocFlags::Reserve))
		{
			allocation_type |= MEM_RESERVE;
		}
		void* return_ptr = ::VirtualAlloc(ptr, size, allocation_type, 0);

		if (return_ptr == nullptr)
		{
			std::runtime_error("Invalid virtual allocation");
		}

		return return_ptr;
	}

	void VirtualFree(void * ptr, size_t size, FreeFlags flags)
	{
		DWORD free_type = 0;
		if (check_flag(flags, FreeFlags::Decommit))
		{
			free_type |= MEM_DECOMMIT;
		}
		if (check_flag(flags, FreeFlags::Release))
		{
			free_type |= MEM_RESERVE;
		}

		if (!::VirtualFree(ptr, size, free_type))
		{
			std::runtime_error("Invalid virtual free");
		}
	}

	size_t GetPageSize()
	{
		if (g_cached_page_size == 0)
		{
			SYSTEM_INFO sSysInfo;         // Useful information about the system

			GetSystemInfo(&sSysInfo);     // Initialize the structure.
			g_cached_page_size = sSysInfo.dwPageSize;
		}

		return g_cached_page_size;
	}
}