//////////////////////////////////////////////////////////////////////////
// Cute engine - Counters for collecting stats
//////////////////////////////////////////////////////////////////////////

#ifndef COUNTERS_H_
#define COUNTERS_H_

#ifndef COUNTERS_ENABLE
#define COUNTERS_ENABLE 1
#endif

#if COUNTERS_ENABLE == 1

#include <stdint.h>
#include <core/string_hash.h>

namespace core
{
	using CounterGroupName = StringHash32<"CounterGroupName"_namespace>;
	using CounterName = StringHash32<"CounterName"_namespace>;

	enum class CounterType
	{
		Main,
		Render
	};

	//Counter marker
	class CounterMarker
	{
	public:
		CounterMarker(const CounterGroupName& group, const CounterName& name, const CounterType& type, bool reset_each_frame);
		void Set(const uint32_t& value);
		void Add(const uint32_t& value);
	private:
		uint32_t index :30;
		uint32_t reset : 1; //Needs to reset each frame
		uint32_t render_counter : 1; //Indicates that is a render counter
	};

	//Update counters in the main tick
	void UpdateCountersMain();
	//Update counters for the render
	void UpdateCountersRender();
	//Display IMGUI dialog with the counters
	bool RenderCounters();
}

#define COUNTER(variable, group, name, reset_each_frame) static core::CounterMarker g_counter_marker_##variable(group##_sh32, name##_sh32, core::CounterType::Main, reset_each_frame);
#define COUNTER_RENDER(variable, group, name, reset_each_frame) static core::CounterMarker g_counter_marker_##variable(group##_sh32, name##_sh32, core::CounterType::Render, reset_each_frame);

#define COUNTER_SET(variable, value) g_counter_marker_##variable.Set(value);
#define COUNTER_INC(variable) g_counter_marker_##variable.Add(1);									
#define COUNTER_SUB(variable) g_counter_marker_##variable.Add(-1);	
#define COUNTER_INC_VALUE(variable, value) g_counter_marker_##variable.Add(value);									
#define COUNTER_SUB_VALUE(variable, value) g_counter_marker_##variable.Add(-value);									

#else
#define COUNTER(variable, group, name, reset_each_frame)
#define COUNTER_RENDER(variable, group, name, reset_each_frame)

#define COUNTER_SET(variable, value)
#define COUNTER_INC(variable)								
#define COUNTER_SUB(variable)	
#define COUNTER_INC_VALUE(variable, value)								
#define COUNTER_SUB_VALUE(variable, value)	
#endif
#endif //COUNTERS_H_
