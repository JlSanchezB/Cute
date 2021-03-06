#include "virtual_buffer.h"
#include <core/virtual_alloc.h>
#include <algorithm>

namespace
{
	//Calculate the page that represent the memory_offset
	size_t calculate_page(size_t memory_offset, size_t page_size)
	{
		if (memory_offset == 0)
		{
			return 0;
		}
		else
		{
			return ((memory_offset - 1) / page_size) + 1;
		}
	}
}

namespace core
{
	VirtualBuffer::VirtualBuffer(size_t reserved_memory)
	{
		if (reserved_memory == 0)
		{
			m_memory_base = nullptr;
		}
		else
		{
			//Need to round the memory to page size
			size_t page_size = GetPageSize();
			reserved_memory = ((reserved_memory / page_size) + 1) * page_size;

			//Reserve memory
			m_memory_base = static_cast<char*>(VirtualAlloc(nullptr, reserved_memory, AllocFlags::Reserve));
		}

		//Non commited memory
		m_memory_commited = 0;
	}

	VirtualBuffer::~VirtualBuffer()
	{
		if (m_memory_base)
		{
			//Deallocate all commited/reserved memory
			VirtualFree(m_memory_base, 0, FreeFlags::Release);
		}
	}

	void VirtualBuffer::SetCommitedSize(size_t new_size, bool free_memory)
	{
		size_t page_size = GetPageSize();

		if (!free_memory)
		{
			new_size = std::max(new_size, m_memory_commited);
		}

		//Check if the new size means new pages to be commited or released
		size_t page_index = calculate_page(m_memory_commited, page_size);
		size_t new_page_index = calculate_page(new_size, page_size);

		if (new_size == 0)
		{
			//Decommit all the memory
			VirtualFree(m_memory_base, m_memory_commited, FreeFlags::Decommit);
		}
		else if (m_memory_commited == 0 && page_index == new_page_index)
		{
			//We need to commit the first one
			VirtualAlloc(m_memory_base, page_size, AllocFlags::Commit);
		}
		else if (page_index == new_page_index)
		{
			//We don't need to update the commited pages
		}
		else if (new_page_index > page_index)
		{
			//We need to commit new pages (from page_index to new_page_index)
			//If there is not sufficient reserved memory, it will fail
			size_t num_pages_to_commit = new_page_index - page_index;
			VirtualAlloc(m_memory_base + page_index * page_size, num_pages_to_commit * page_size, AllocFlags::Commit);
		}
		else
		{
			//We need to decommit the memory from (new_page_index + 1) to page_index (included page_index)
			size_t num_pages_to_decommit = page_index - new_page_index;
			VirtualFree(m_memory_base + (new_page_index + 1) * page_size, num_pages_to_decommit * page_size, FreeFlags::Decommit);
		}

		//Update commited memory value
		m_memory_commited = new_size;
	}
}