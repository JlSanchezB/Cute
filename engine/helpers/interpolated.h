#ifndef INTERPOLATED_H_
#define INTERPOLATED_H_

#include <array>
#include <ext/glm/glm.hpp>
#include <ext/glm/ext.hpp>

namespace helpers
{
	//Lerp helper
	template<class T>
	T Lerp(const T& a, const T& b, const float t)
	{
		//Mix can implement a lot of them
		return glm::mix(a, b, t);
	}

	//Specialized for quats, as it needs lerp
	template<>
	inline glm::quat Lerp(const glm::quat& a, const glm::quat& b, const float t)
	{
		return glm::lerp(a, b, t);
	}


	//CONTROL class defines 
	// s_frame, current frame
	// s_interpolation_value, interpolation value between the two frames
	// s_interpolate_phase, we expect access to interpolated data
	// s_update_phase, we expect access to update data

	//Helper class to interpolate between two frames
	template<class DATA, class CONTROL>
	class Interpolated
	{
	public:
		//Reset the last and the current to the same value, used for teleporting and other kind of effects
		void Reset(const DATA& value)
		{
			assert(CONTROL::s_update_phase);
			m_data[0] = m_data[1] = value;
		}
		//Returns interpolated value
		DATA GetInterpolated() const
		{
			assert(CONTROL::s_interpolate_phase);
			return Lerp(m_data[(CONTROL::s_frame + 1) % 2], m_data[CONTROL::s_frame], CONTROL::s_interpolation_value);
		}

		//Returns current value in update phase
		DATA& operator*()
		{
			assert(CONTROL::s_update_phase);
			return m_data[CONTROL::s_frame];
		}
		const DATA& operator*() const
		{
			assert(CONTROL::s_update_phase);
			return m_data[CONTROL::s_frame];
		}

		//Retuns last frame update value
		const DATA& Last() const
		{
			assert(CONTROL::s_update_phase);
			return m_data[(CONTROL::s_frame + 1) % 2];
		}
		
	
	private:
		//Two values stored, the CONTROL class will be use to decide the current index and the interpolation value
		std::array<DATA, 2> m_data;
	};
}

#endif //INTERPOLATED_H