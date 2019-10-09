//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a ring buffer
//////////////////////////////////////////////////////////////////////////

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <array>
#include <cassert>

namespace core
{
	template <typename DATA, size_t SIZE>
	class RingBuffer
	{
	public:
		//Default constructor
		RingBuffer();

		//Free all memory
		~RingBuffer();

		//Return if it is full
		bool full() const;

		//Return if it is empty
		bool empty() const;

		//emplace to the tail
		template<typename ...Args>
		void emplace(Args&&... args);

		//Get the tail
		DATA& tail();

		//Get the head
		DATA& head();

		//pop head
		void pop();

	private:
		static_assert(SIZE >= 2);

		//Indexes
		size_t m_head_index;
		size_t m_tail_index;

		//Data
		std::array<typename std::aligned_storage<sizeof(DATA), alignof(DATA)>::type, SIZE> m_buffer;
	};

	template<typename DATA, size_t SIZE>
	inline RingBuffer<DATA, SIZE>::RingBuffer()
	{
		m_head_index = 0;
		m_tail_index = 0;
	}

	template<typename DATA, size_t SIZE>
	inline RingBuffer<DATA, SIZE>::~RingBuffer()
	{
		while (!empty())
		{
			pop();
		}
	}

	template<typename DATA, size_t SIZE>
	inline bool RingBuffer<DATA, SIZE>::full() const
	{
		return (m_tail_index + 1 + SIZE) % SIZE == m_head_index;
	}

	template<typename DATA, size_t SIZE>
	inline bool RingBuffer<DATA, SIZE>::empty() const
	{
		return m_head_index == m_tail_index;
	}

	template<typename DATA, size_t SIZE>
	template<typename ...Args>
	inline void RingBuffer<DATA, SIZE>::emplace(Args && ...args)
	{
		assert(!full());

		new(&m_buffer[m_tail_index]) DATA(std::forward<Args>(args)...);

		//Increase tail
		m_tail_index = (m_tail_index + 1) % SIZE;
	}

	template<typename DATA, size_t SIZE>
	inline DATA & RingBuffer<DATA, SIZE>::head()
	{
		return *reinterpret_cast<DATA*>(&m_buffer[m_head_index]);
	}

	template<typename DATA, size_t SIZE>
	inline DATA& RingBuffer<DATA, SIZE>::tail()
	{
		return *reinterpret_cast<DATA*>(&m_buffer[m_tail_index]);
	}

	template<typename DATA, size_t SIZE>
	inline void RingBuffer<DATA, SIZE>::pop()
	{
		assert(!empty());
		
		//Destroy the head
		reinterpret_cast<DATA*>(&m_buffer[m_head_index])->~DATA();

		//Increase the head
		m_head_index = (m_head_index + 1) % SIZE;
	}


	template <typename DATA>
	class GrowableRingBuffer
	{
	public:
		//Default constructor
		GrowableRingBuffer(size_t init_size = 16);

		//Free all memory
		~GrowableRingBuffer();

		//Return if it is empty
		bool empty() const;

		//emplace to the tail
		template<typename ...Args>
		void emplace(Args&&... args);

		//Get the tail
		DATA& tail();

		//Get the head
		DATA& head();

		//pop head
		void pop();

		//reserve size
		size_t reserved_size() const;

	private:

		//Indexes
		size_t m_head_index;
		size_t m_tail_index;

		//Data
		std::vector<typename std::aligned_storage<sizeof(DATA), alignof(DATA)>::type> m_buffer;
	};

	template<typename DATA>
	inline GrowableRingBuffer<DATA>::GrowableRingBuffer(size_t init_size) : m_buffer(init_size)
	{
		assert(init_size > 0);

		m_head_index = 0;
		m_tail_index = 0;
	}

	template<typename DATA>
	inline GrowableRingBuffer<DATA>::~GrowableRingBuffer()
	{
		while (!empty())
		{
			pop();
		}
	}

	template<typename DATA>
	inline bool GrowableRingBuffer<DATA>::empty() const
	{
		return m_head_index == m_tail_index;
	}

	template<typename DATA>
	template<typename ...Args>
	inline void GrowableRingBuffer<DATA>::emplace(Args&& ...args)
	{
		assert(m_buffer.size() > 0);

		//Check if we need to grow
		size_t old_buffer_size = m_buffer.size();
		if ((m_tail_index + 1 + old_buffer_size) % old_buffer_size == m_head_index)
		{
			//We are going to create a new buffer and move the old one
			std::vector<typename std::aligned_storage<sizeof(DATA), alignof(DATA)>::type> new_buffer(old_buffer_size * 2);

			size_t dest_index = 0;
			for (size_t src_index = m_head_index; src_index != m_tail_index; src_index = (src_index + 1 + old_buffer_size) % old_buffer_size)
			{
				//Move data to the new vector
				*reinterpret_cast<DATA*>(&new_buffer[dest_index]) = std::move(*reinterpret_cast<DATA*>(&m_buffer[src_index]));

				dest_index++;
			}

			//Move
			m_head_index = 0;
			m_tail_index = old_buffer_size;
			m_buffer = std::move(new_buffer);
		}

		new(&m_buffer[m_tail_index]) DATA(std::forward<Args>(args)...);

		//Increase tail
		m_tail_index = (m_tail_index + 1) % m_buffer.size();
	}

	template<typename DATA>
	inline DATA& GrowableRingBuffer<DATA>::head()
	{
		return *reinterpret_cast<DATA*>(&m_buffer[m_head_index]);
	}

	template<typename DATA>
	inline DATA& GrowableRingBuffer<DATA>::tail()
	{
		return *reinterpret_cast<DATA*>(&m_buffer[m_tail_index]);
	}

	template<typename DATA>
	inline void GrowableRingBuffer<DATA>::pop()
	{
		assert(!empty());

		//Destroy the head
		reinterpret_cast<DATA*>(&m_buffer[m_head_index])->~DATA();

		//Increase the head
		m_head_index = (m_head_index + 1) % m_buffer.size();
	}

	template<typename DATA>
	inline size_t GrowableRingBuffer<DATA>::reserved_size() const
	{
		return m_buffer.size();
	}
}

#endif //RING_BUFFER_H_