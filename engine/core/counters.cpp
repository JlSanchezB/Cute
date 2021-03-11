#include "counters.h"
#include "virtual_buffer.h"
#include "fast_map.h"
#include <atomic>
#include <array>
#include <core/log.h>
#include <ext/imgui/imgui.h>

namespace
{
	struct CounterReset
	{
		std::atomic<uint32_t>* atomics[2];
	};

	struct Counter
	{
		std::atomic<uint32_t>* atomic;
	};

	struct Group
	{
		core::FastMap <core::CounterName, Counter> counters;
		core::FastMap <core::CounterName, CounterReset> counters_reset;
	};

	struct CounterManager
	{
		core::VirtualBufferInitied<1024 * 1024> atomics_buffer;

		core::FastMap<core::CounterGroupName, Group> main_groups;
		core::FastMap<core::CounterGroupName, Group> render_groups;

		uint32_t main_index = 0;
		uint32_t render_index = 0;

		size_t number_of_atomics = 0;

		std::atomic<uint32_t>& GetAtomic(const size_t& index)
		{
			std::atomic<uint32_t>* buffer = reinterpret_cast<std::atomic<uint32_t>*>(atomics_buffer.GetPtr());
			return buffer[index];
		}
	};

	std::unique_ptr<CounterManager> g_counter_manager;

	constexpr size_t atomic_size = sizeof(std::atomic<uint32_t>);
}

namespace core
{
	CounterMarker::CounterMarker(const CounterGroupName& group, const CounterName& name, const CounterType& type, bool reset_each_frame)
	{
		if (!g_counter_manager)
		{
			//Create it
			g_counter_manager = std::make_unique<CounterManager>();
		}
		//Register the counter 

		//Reserve space
		g_counter_manager->atomics_buffer.SetCommitedSize((g_counter_manager->number_of_atomics + (reset_each_frame)?2:1) * atomic_size);

		//The index represent the first of the to atomics
		index = g_counter_manager->number_of_atomics;
		render_counter = (type == CounterType::Render);
		reset = reset_each_frame;

		//Placement new the atomics into the atomic buffer
		std::atomic<uint32_t>* atomic_one = new (reinterpret_cast<uint8_t*>(g_counter_manager->atomics_buffer.GetPtr()) + atomic_size * index) std::atomic<uint32_t>(0);
		std::atomic<uint32_t>* atomic_two = nullptr;
		if (reset_each_frame)
		{
			atomic_two = new (reinterpret_cast<uint8_t*>(g_counter_manager->atomics_buffer.GetPtr()) + atomic_size * (index + 1)) std::atomic<uint32_t>(0);
		}

		g_counter_manager->number_of_atomics += (reset_each_frame) ? 2 : 1;

		//Add the counter into the map

		//Look for the group
		auto& group_map = (type == CounterType::Main) ? g_counter_manager->main_groups : g_counter_manager->render_groups;
		auto& group_it = group_map.Find(group);

		if (!group_it)
		{
			//Create the group
			group_it = group_map.Insert(group, Group{});
		}

		auto& counter_it = group_it->counters.Find(name);
		if (counter_it)
		{
			core::LogError("Counter <%s> is already defined in the group <%s>", name.GetValue(), group.GetValue());
			return;
		}
		//Add
		if (reset_each_frame)
		{
			group_it->counters_reset.Insert(name, CounterReset{ {atomic_one, atomic_two} });
		}
		else
		{
			group_it->counters.Insert(name, Counter{ {atomic_one} });
		}
	}

	void CounterMarker::Set(const uint32_t& value)
	{
		size_t atomic_index = (!reset) ? index : index + (render_counter) ? g_counter_manager->render_index : g_counter_manager->main_index;
		g_counter_manager->GetAtomic(atomic_index).exchange(value);
	}

	void CounterMarker::Add(const uint32_t& value)
	{
		size_t atomic_index = (!reset) ? index : index + (render_counter) ? g_counter_manager->render_index : g_counter_manager->main_index;
		g_counter_manager->GetAtomic(atomic_index).fetch_add(value);
	}

	void core::UpdateCountersMain()
	{
		if (g_counter_manager)
		{
			g_counter_manager->main_index = (g_counter_manager->main_index + 1) % 2;

			//Reset counters if needed
			for (auto& group : g_counter_manager->main_groups)
			{
				for (auto& counter : group.second.counters_reset)
				{
					counter.second.atomics[g_counter_manager->main_index]->exchange(0);
				}
			}
		}
	}

	void core::UpdateCountersRender()
	{
		if (g_counter_manager)
		{
			g_counter_manager->render_index = (g_counter_manager->render_index + 1) % 2;

			//Reset counters if needed
			for (auto& group : g_counter_manager->render_groups)
			{
				for (auto& counter : group.second.counters_reset)
				{
					counter.second.atomics[g_counter_manager->render_index]->exchange(0);
				}
			}
		}
	}

	bool core::RenderCounters()
	{
		if (!g_counter_manager)
			return false;

		bool activated = true;
		//For each group
		if (ImGui::Begin("Counters", &activated))
		{
			//Render the frame not used
			size_t main_frame = g_counter_manager->main_index;
			size_t render_frame = (g_counter_manager->render_index + 1) % 2;

			//Add tree node for each group
			for (auto& counter_group : g_counter_manager->main_groups)
			{
				if (ImGui::TreeNode(counter_group.first.GetValue()))
				{
					//Add a slot for each variable
					for (auto& counter : counter_group.second.counters)
					{
						uint32_t value = counter.second.atomic->load();
						ImGui::Text("%s = %u", counter.first.GetValue(), value);
					}
					for (auto& counter : counter_group.second.counters_reset)
					{
						uint32_t value = counter.second.atomics[main_frame]->load();
						ImGui::Text("%s = %u", counter.first.GetValue(), value);
					}
					ImGui::TreePop();
				}
			}
			for (auto& counter_group : g_counter_manager->render_groups)
			{
				if (ImGui::TreeNode(counter_group.first.GetValue()))
				{
					//Add a slot for each variable
					for (auto& counter : counter_group.second.counters)
					{
						uint32_t value = counter.second.atomic->load();
						ImGui::Text("%s = %u", counter.first.GetValue(), value);
					}
					for (auto& counter : counter_group.second.counters_reset)
					{
						uint32_t value = counter.second.atomics[render_frame]->load();
						ImGui::Text("%s = %u", counter.first.GetValue(), value);
					}
					ImGui::TreePop();
				}
			}

		}
		ImGui::End();

		return activated;
	}
};
