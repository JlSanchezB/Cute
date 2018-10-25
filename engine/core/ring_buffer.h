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

		//Return if it is empty
		bool empty() const;

		//emplace to the tail
		template<typename ...Args>
		void emplace(Args&&... args);

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
		std::array<DATA, SIZE> m_buffer;
	};

	template<typename DATA, size_t SIZE>
	inline RingBuffer<DATA, SIZE>::RingBuffer()
	{
		m_head_index = 0;
		m_tail_index = 0;
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
		return m_buffer[m_head_index];
	}

	template<typename DATA, size_t SIZE>
	inline void RingBuffer<DATA, SIZE>::pop()
	{
		assert(!empty());
		
		//Destroy the head
		m_buffer[m_head_index].~DATA();

		//Increase the head
		m_head_index = (m_head_index + 1) % SIZE;
	}
}

#endif //RING_BUFFER_H_