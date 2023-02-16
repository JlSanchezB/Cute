//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation for a simple slot pool
//////////////////////////////////////////////////////////////////////////

#ifndef SLOT_POOL_H_
#define SLOT_POOL_H_

#include <array>
#include <cassert>
#include <vector>

namespace core
{
	template <typename TYPE>
	class Slot
	{
		static constexpr TYPE kInvalid = static_cast<TYPE>(-1);
		TYPE m_index = kInvalid;

		//Private constructor of a handle, only a slot pool can create one
		Slot(TYPE index)
		{
			m_index = index;
		}
		template <typename TYPE, TYPE MAX_SIZE, size_t MAX_FRAMES>
		friend class SlotPool;
	public:

		//Public constructor
		Slot()
		{
			m_index = kInvalid;
		}

		//Disable copy construction and copy assing, only is allowed to move
		Slot(const Slot& a) = delete;

		Slot(Slot&& a)
		{
			//The destination handle needs to be invalid
			assert(m_index == kInvalid);
			m_index = a.m_index;
			a.m_index = kInvalid;
		}

		Slot& operator=(const Slot& a) = delete;

		Slot& operator=(Slot&& a)
		{
			//The destination handle needs to be invalid
			assert(m_index == kInvalid);
			m_index = a.m_index;
			a.m_index = kInvalid;
			return *this;
		}

		~Slot()
		{
			//Only invalid index can be destructed, if not we will have leaks
			assert(m_index == kInvalid);
		}

		TYPE GetIndex() const
		{
			return m_index;
		}
	};

	//And slot pool with give you back and ID that can be use for identify something and it will keep it alive until the correct frame
	template <typename TYPE, TYPE MAX_SIZE, size_t MAX_FRAMES = 8>
	class SlotPool
	{
		//List of free slots
		std::vector<Slot<TYPE>> m_free_slots;

		//Size
		TYPE m_size;

		static constexpr size_t kInvalidFrame = static_cast<size_t>(-1);
		struct Frame
		{
			size_t frame_index = kInvalidFrame;
			std::vector<Slot<TYPE>> deferred_free_slots;
		};

		std::array<Frame, MAX_FRAMES> m_frames;

		size_t m_current_frame = kInvalidFrame;

	public:
		SlotPool()
		{
			m_size = 0;
		}

		~SlotPool()
		{
		}

		Slot<TYPE> Alloc()
		{
			assert(m_current_frame != kInvalidFrame);

			//Check for a free slot
			if (m_free_slots.size() > 0)
			{
				Slot<TYPE> new_index = std::move(m_free_slots.back());
				m_free_slots.pop_back();
				return std::move(new_index);
			}

			if (m_size < MAX_SIZE)
			{
				return Slot<TYPE>(m_size++);
			}

			//Error, no more slots
			throw std::runtime_error::exception("Slot pool is full");

			return Slot<TYPE>();
		}

		void Free(Slot<TYPE>& slot)
		{
			assert(m_current_frame != kInvalidFrame);
			
			//Add it to the current frame for deferred allocation
			m_frames[m_current_frame % MAX_FRAMES].deferred_free_slots.push_back(std::move(slot));	
		}

		void Init(size_t current_frame_index)
		{
			assert(m_current_frame == kInvalidFrame);
			m_current_frame = current_frame_index;
		}

		//New frame, delete all from freed frame and before
		void Sync(size_t current_frame_index, size_t freed_frame_index)
		{
			//Free all allocations until freed_frame_index
			for (size_t i = 0; i < MAX_FRAMES; ++i)
			{
				auto& frame = m_frames[i];
				if (frame.frame_index != kInvalidFrame && frame.frame_index <= freed_frame_index)
				{
					//Free the slots
					for (auto& free_slot : frame.deferred_free_slots)
					{
						m_free_slots.push_back(std::move(free_slot));
					}
					frame.deferred_free_slots.clear();
				}
			}

			m_current_frame = current_frame_index;
			m_frames[m_current_frame % MAX_FRAMES].frame_index = m_current_frame;
			//The current frame should be free
			assert(m_frames[m_current_frame % MAX_FRAMES].deferred_free_slots.size() == 0);
		}
	};
}

#endif
