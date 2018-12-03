#include <stdarg.h>

namespace
{
	inline void AddError(render::LoadContext& load_context, const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		vsnprintf_s(buffer, 1024, 1024 - 1, message, args);
		va_end(args);

		load_context.errors.push_back(buffer);
	}

	enum class AttributeType
	{
		Optional,
		NonOptional
	};

	//Template interface to Query a value from a XML element
	template <typename TYPE>
	bool QueryAttribute(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, TYPE& value, AttributeType attribute_type)
	{
		if (!xml_element->QueryAttribute(name, &value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>", name, load_context.name);
				return false;
			}
		}
		return true;
	}

	template <>
	bool QueryAttribute<size_t>(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, size_t& value, AttributeType attribute_type)
	{
		int64_t int64_value;
		if (QueryAttribute(load_context, xml_element, name, int64_value, attribute_type))
		{
			value = static_cast<size_t>(int64_value);
			return true;
		}
		else
		{
			return false;
		}
	}

	//Template interface to Query a value and match in a table for a XML element
	template <typename TYPE, typename CONVERSION_TABLE>
	bool QueryTableAttribute(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, TYPE& value, const CONVERSION_TABLE& conversion_table, AttributeType attribute_type)
	{
		const char* string_value;
		if (!xml_element->QueryStringAttribute(name, &string_value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>", name, load_context.name);
				return false;
			}
		}
		else
		{
			//Convert the table
			//Check the conversion table for it
			for (size_t i = 0; i < std::size(conversion_table); ++i)
			{
				auto& pair = conversion_table[i];
				if (strcmp(pair.first, string_value) == 0)
				{
					//found
					value = pair.second;
					return true;
				}
			}
			AddError(load_context, "Error converting value <%s> in attribute <%s> in node <%s>", string_value, name, load_context.name);
			return false;
		}
		return true;
	}
}
