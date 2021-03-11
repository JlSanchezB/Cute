//////////////////////////////////////////////////////////////////////////
// Cute engine - Control variables
//////////////////////////////////////////////////////////////////////////

#include "control_variables.h"
#include "fast_map.h"
#include "log.h"
#include <ext/imgui/imgui.h>

namespace
{
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

	using SupportedControlVariable = std::variant<int32_t*, uint32_t*, bool*, float*>;
	struct ControlVariableValue
	{
		union
		{
			int32_t value_int32_t;
			uint32_t value_uint32_t;
			bool value_bool;
			float value_float;
		};

		ControlVariableValue(int32_t value) : value_int32_t(value) {};
		ControlVariableValue(uint32_t value) : value_uint32_t(value) {};
		ControlVariableValue(bool value) : value_bool(value) {};
		ControlVariableValue(float value) : value_float(value) {};

	};

	struct ControlVariable
	{
		SupportedControlVariable reference;
		core::ConsoleVariableType type;
		ControlVariableValue range_min;
		ControlVariableValue range_max;
	};

	//Storage of all the pointers to control variables
	struct ControlVariableGroup
	{
		core::FastMap<core::ControlVariableName, ControlVariable> control_variables;
	};

	//Control variable map
	core::FastMap<core::ControlVariableGroupName, ControlVariableGroup>* g_control_variables = nullptr;

	//Updates pending
	std::vector<std::pair<SupportedControlVariable, ControlVariableValue>> g_pending_updates_main;
	std::vector<std::pair<SupportedControlVariable, ControlVariableValue>> g_pending_updates_render;


	void RegisterVariable(const SupportedControlVariable& variable, const ControlVariableValue& range_min, const ControlVariableValue& range_max, core::ControlVariableGroupName group_name, core::ControlVariableName variable_name, const core::ConsoleVariableType& type)
	{
		//Create the map if needed
		if (g_control_variables == nullptr)
		{
			g_control_variables = new core::FastMap<core::ControlVariableGroupName, ControlVariableGroup>;
		}

		//Look for the group
		auto& group_it = g_control_variables->Find(group_name);

		if (!group_it)
		{
			//Create the group
			group_it = g_control_variables->Insert(group_name, ControlVariableGroup{});
		}

		auto& control_variable_it = group_it->control_variables.Find(variable_name);
		if (control_variable_it)
		{
			core::LogError("Control variable <%s> is already defined in the group <%s>", variable_name.GetValue(), group_name.GetValue());
			return;
		}
		//Add
		group_it->control_variables.Insert(variable_name, ControlVariable{ variable, type, range_min, range_max });
	}

	void UpdateControlVariables(std::vector<std::pair<SupportedControlVariable, ControlVariableValue>>& pending_updates)
	{
		for (auto& variable : pending_updates)
		{
			std::visit(
				overloaded
				{
					[&](int32_t* variable_ptr)
					{
						*variable_ptr = variable.second.value_int32_t;
					},
					[&](uint32_t* variable_ptr)
					{
						*variable_ptr = variable.second.value_uint32_t;
					},
					[&](bool* variable_ptr)
					{
						*variable_ptr = variable.second.value_bool;
					},
					[&](float* variable_ptr)
					{
						*variable_ptr = variable.second.value_float;
					}
				},
				variable.first);
		}
	}

	template <typename TYPE>
	void RenderScalarControlVariable(const core::ControlVariableName& name, TYPE* control_variable, const TYPE& min_range, const TYPE& max_range, const core::ConsoleVariableType& type)
	{
		float drag_speed = std::max(0.2f, (static_cast<float>(max_range) - static_cast<float>(min_range)) * 0.0001f);
		TYPE value = *control_variable;
		if (ImGui::DragScalar(name.GetValue(), ImGuiDataType_S32, &value, drag_speed, &min_range, &max_range))
		{
			//Changed
			if (type == core::ConsoleVariableType::Main)
			{
				g_pending_updates_main.push_back({ control_variable, value });
			}
			else
			{
				g_pending_updates_render.push_back({ control_variable, value });
			}
		}
	}
}

namespace core
{
	int32_t RegisterControlVariable(const int32_t& default_value, int32_t* variable_ptr, const int32_t& range_min, const int32_t& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type)
	{
		RegisterVariable(variable_ptr, range_min, range_max, group_name, variable_name, type);

		return default_value;
	}

	uint32_t RegisterControlVariable(const uint32_t& default_value, uint32_t* variable_ptr, const uint32_t& range_min, const uint32_t& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type)
	{
		RegisterVariable(variable_ptr, range_min, range_max, group_name, variable_name, type);

		return default_value;
	}

	bool RegisterControlVariable(const bool& default_value, bool* variable_ptr, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type)
	{
		RegisterVariable(variable_ptr, false, true, group_name, variable_name, type);

		return default_value;
	}

	float RegisterControlVariable(const float& default_value, float* variable_ptr, const float& range_min, const float& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type)
	{
		RegisterVariable(variable_ptr, range_min, range_max, group_name, variable_name, type);

		return default_value;
	}

	void UpdateControlVariablesMain()
	{
		UpdateControlVariables(g_pending_updates_main);
	}

	void UpdateControlVariablesRender()
	{
		UpdateControlVariables(g_pending_updates_render);
	}

	

	void DestroyControlVariables()
	{
		//Cleat the map
		delete g_control_variables;
		g_control_variables = nullptr;
	}

	bool RenderControlVariables()
	{
		bool activated = true;
		//For each group
		if (ImGui::Begin("Control Variables", &activated))
		{
			//Add tree node for each group
			for (auto& control_variable_group : *g_control_variables)
			{
				if (ImGui::TreeNode(control_variable_group.first.GetValue()))
				{
					//Add a slot for each variable
					for (auto& control_variable : control_variable_group.second.control_variables)
					{
						std::visit(
							overloaded
							{
								[&](int32_t* variable_ptr)
								{
									RenderScalarControlVariable<int32_t>(control_variable.first, variable_ptr, control_variable.second.range_min.value_int32_t, control_variable.second.range_max.value_int32_t, control_variable.second.type);
								},
								[&](uint32_t* variable_ptr)
								{
									RenderScalarControlVariable<uint32_t>(control_variable.first, variable_ptr, control_variable.second.range_min.value_uint32_t, control_variable.second.range_max.value_uint32_t, control_variable.second.type);
								},
								[&](bool* variable_ptr)
								{
									bool value = *variable_ptr;
									if (ImGui::Checkbox(control_variable.first.GetValue(), &value))
									{
										//Changed
										if (control_variable.second.type == core::ConsoleVariableType::Main)
										{
											g_pending_updates_main.push_back({ control_variable.second.reference, value });
										}
										else
										{
											g_pending_updates_render.push_back({ control_variable.second.reference, value });
										}
									}
								},
								[&](float* variable_ptr)
								{
									RenderScalarControlVariable<float>(control_variable.first, variable_ptr, control_variable.second.range_min.value_float, control_variable.second.range_max.value_float, control_variable.second.type);
								}
							},
							control_variable.second.reference);
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();

		return activated;
	}
}