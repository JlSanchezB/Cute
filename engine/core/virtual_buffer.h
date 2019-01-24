//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a virtual memory buffer
// It reserves a virtual memory chunk of memory and commits memory as it grows
//////////////////////////////////////////////////////////////////////////

#ifndef VIRTUAL_BUFFER_H_
#define VIRTUAL_BUFFER_H_

namespace core
{
	class VirtualBuffer
	{
	public:
		//Reserved memory is defined during construction
		explicit VirtualBuffer(size_t reserved_memory);
		~VirtualBuffer();

		//Set the memory that needs to be commited
		void SetCommitedSize(size_t new_size);

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
}

#endif //VIRTUAL_BUFFER_H_
