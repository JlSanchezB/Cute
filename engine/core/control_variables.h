//////////////////////////////////////////////////////////////////////////
// Cute engine - Control variables
//////////////////////////////////////////////////////////////////////////

#ifndef CONTROL_VARIABLES_H_
#define CONTROL_VARIABLES_H_

#include <variant>
#include <core/string_hash.h>

namespace core
{
	using ControlVariableGroupName = StringHash32<"ControlVariableGroupName"_namespace>;
	using ControlVariableName = StringHash32<"ControlVariableName"_namespace>;
	
	enum class ConsoleVariableType
	{
		Main, //Gets updated during the begin of the main tick
		Render //Gets updated during the render tick
	};
	//Supported registers
	int32_t RegisterControlVariable(const int32_t& default_value, int32_t* variable_ptr, const int32_t& range_min, const int32_t& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type);
	uint32_t RegisterControlVariable(const uint32_t& default_value, uint32_t* variable_ptr, const uint32_t& range_min, const uint32_t& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type);
	bool RegisterControlVariable(const bool& default_value, bool* variable_ptr, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type);
	float RegisterControlVariable(const float& default_value, float* variable_ptr, const float& range_min, const float& range_max, const ControlVariableGroupName& group_name, const ControlVariableName& variable_name, const ConsoleVariableType& type);

	//Update pending variables for the main tick
	void UpdateControlVariablesMain();
	//Update pending variables for the render
	void UpdateControlVariablesRender();
	//Destroy control variables
	void DestroyControlVariables();
	//Display IMGUI dialog with the control variables
	bool RenderControlVariables();
}

#define CONTROL_VARIABLE_BOOL(variable, default_value, group_name, variable_name) static bool variable = RegisterControlVariable(default_value, &variable, group_name##_sh32, variable_name##_sh32, core::ConsoleVariableType::Main); 
#define CONTROL_VARIABLE_BOOL_RENDER(variable, default_value, group_name, variable_name) static bool variable = RegisterControlVariable(default_value, &variable, group_name##_sh32, variable_name##_sh32, core::ConsoleVariableType::Render); 
#define CONTROL_VARIABLE(type, variable, range_min, range_max, default_value, group_name, variable_name) static type variable = RegisterControlVariable(static_cast<type>(default_value), &variable, static_cast<type>(range_min), static_cast<type>(range_max), group_name##_sh32, variable_name##_sh32, core::ConsoleVariableType::Main); 
#define CONTROL_VARIABLE_RENDER(type, variable, default_value, range_min, range_max, group_name, variable_name) static type variable = RegisterControlVariable(static_cast<type>(default_value), &variable, static_cast<type>(range_min), static_cast<type>(range_max), group_name##_sh32, variable_name##_sh32, core::ConsoleVariableType::Render); 

#endif //CONTROL_VARIABLES_H_
