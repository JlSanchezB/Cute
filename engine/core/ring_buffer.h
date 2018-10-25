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

		//Return if it is full
		bool full() const;

		//emplace
		template<typename ...Args>
		void emplace(Args&&... args);

		//pop
		DATA pop();

	private:
		static_assert(SIZE >= 2);

		//Indexes
		size_t m_head;
		size_t m_tail;

		//Data
		std::array<DATA, SIZE> m_buffer;
	};

	template<typename DATA, size_t SIZE>
	inline RingBuffer<DATA, SIZE>::RingBuffer()
	{
		m_head = 0;
		m_tail = 1;
	}

	template<typename DATA, size_t SIZE>
	inline bool RingBuffer<DATA, SIZE>::full() const
	{
		return m_tail == m_head;
	}

	template<typename DATA, size_t SIZE>
	template<typename ...Args>
	inline void RingBuffer<DATA, SIZE>::emplace(Args && ...args)
	{
		assert(!full());

		//Create in tail
		m_buffer[m_tail] = DATA(args...);

		//Increase tail
		m_tail = (m_tail + 1) % SIZE;
	}

	template<typename DATA, size_t SIZE>
	inline DATA RingBuffer<DATA, SIZE>::pop()
	{
		assert(m_head != (m_tail - 1 + SIZE) % SIZE);
		//Get data
		DATA ret = m_buffer[m_head];

		//Increase the head
		m_head = (m_head + 1) % SIZE;

		return ret;
	}
}

#endif //RING_BUFFER_H_