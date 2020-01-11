//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a virtual memory buffer
// It reserves a virtual memory chunk of memory and commits memory as it grows
//////////////////////////////////////////////////////////////////////////

#ifndef VIRTUAL_BUFFER_H_
#define VIRTUAL_BUFFER_H_

#include <cassert>

namespace core
{
	class VirtualBuffer
	{
	public:
		//Reserved memory is defined during construction
		explicit VirtualBuffer(size_t reserved_memory);
		~VirtualBuffer();

		VirtualBuffer(const VirtualBuffer&)
		{
			//Copy constructor can not be use, TODO fix this, that buffers can not be use with vectors containers
			assert(false);
		};
		VirtualBuffer& operator= (const VirtualBuffer&)
		{
			//Copy constructor can not be use
			assert(false);
		};

		//Set the memory that needs to be commited
		void SetCommitedSize(size_t new_size, bool free_memory = true);

		//Get Commited Size
		size_t GetCommitedSize() const
		{
			return m_memory_commited;
		}

		//Get memory
		void* GetPtr() const
		{
			return m_memory_base;
		}

	private:
		//Virtual memory address of the buffer
		char* m_memory_base;
		//Memory commited for this buffer
		size_t m_memory_commited;
	};

	template <size_t RESERVED_MEMORY>
	class VirtualBufferInitied : public VirtualBuffer
	{
	public:
		VirtualBufferInitied() : VirtualBuffer(RESERVED_MEMORY)
		{
		}
	};

	template <typename TYPE>
	class VirtualBufferTyped : VirtualBuffer
	{
	public:
		explicit VirtualBufferTyped(size_t reserved_memory) : VirtualBuffer(reserved_memory)
		{
		}
		void SetSize(const size_t size, bool free_memory = true)
		{
			SetCommitedSize(size * sizeof(TYPE), free_memory);
		}

		TYPE& operator[](const size_t index)
		{
			assert(index < GetSize());

			return (reinterpret_cast<TYPE*>(GetPtr()))[index];
		}

		const TYPE& operator[](const size_t index) const
		{
			assert(index < GetSize());

			return (reinterpret_cast<TYPE*>(GetPtr()))[index];
		}

		size_t GetSize() const
		{
			return GetCommitedSize() / sizeof(TYPE);
		}

		void PushBack(const TYPE& data)
		{
			size_t size = GetSize();
			SetSize(GetSize() + 1);

			(reinterpret_cast<TYPE*>(GetPtr()))[size] = data;
		}
	};

	template <typename TYPE, size_t RESERVED_SIZE>
	class VirtualBufferTypedInitied : public VirtualBufferTyped<TYPE>
	{
	public:
		VirtualBufferTypedInitied() : VirtualBufferTyped(RESERVED_SIZE)
		{
		}
	};
}

#endif //VIRTUAL_BUFFER_H_
