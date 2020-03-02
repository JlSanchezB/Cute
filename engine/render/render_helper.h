#include <stdarg.h>
#include <ext/tinyxml2/tinyxml2.h>

namespace
{
	inline void AddError(render::ErrorContext& load_context, const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		vsnprintf_s(buffer, 1024, 1024 - 1, message, args);
		va_end(args);

		load_context.errors.push_back(buffer);
	}
	template<typename RESOURCE, typename HANDLE, typename ...ARGS>
	inline std::unique_ptr<render::Resource> CreateResourceFromHandle(HANDLE&& handle, ARGS&& ...args)
	{
		std::unique_ptr<RESOURCE> resource_unique = std::make_unique<RESOURCE>(std::forward<ARGS>(args)...);
		resource_unique->Init(handle);
		return resource_unique;
	}

	inline bool CheckNodeName(tinyxml2::XMLElement* xml_element, const char* name)
	{
		return strcmp(xml_element->Name(), name) == 0;
	}

	enum class AttributeType
	{
		Optional,
		NonOptional
	};

	template<typename>
	struct ConversionTable
	{
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
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>, line <%i>", name, load_context.name, xml_element->GetLineNum());
				return false;
			}
		}
		return true;
	}

	template <>
	inline bool QueryAttribute<size_t>(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, size_t& value, AttributeType attribute_type)
	{
		int64_t int64_value;
		if (!xml_element->QueryAttribute(name, &int64_value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>, line <%i>", name, load_context.name, xml_element->GetLineNum());
				return false;
			}
		}
		else
		{
			value = static_cast<size_t>(int64_value);
		}
		return true;
	}

	template <>
	inline bool QueryAttribute<uint8_t>(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, uint8_t& value, AttributeType attribute_type)
	{
		unsigned int unsigned_int_value;
		if (!xml_element->QueryAttribute(name, &unsigned_int_value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>, line <%i>", name, load_context.name, xml_element->GetLineNum());
				return false;
			}
		}
		else
		{
			value = static_cast<uint8_t>(unsigned_int_value);
		}
		return true;
	}

	template <>
	inline bool QueryAttribute<uint16_t>(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, uint16_t& value, AttributeType attribute_type)
	{
		unsigned int unsigned_int_value;
		if (!xml_element->QueryAttribute(name, &unsigned_int_value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>, line <%i>", name, load_context.name, xml_element->GetLineNum());
				return false;
			}
		}
		else
		{
			value = static_cast<uint16_t>(unsigned_int_value);
		}
		return true;
	}

	//Template interface to Query a value and match in a table for a XML element
	template <typename TYPE>
	bool QueryTableAttribute(render::LoadContext& load_context, tinyxml2::XMLElement* xml_element, const char* name, TYPE& value, AttributeType attribute_type)
	{
		const char* string_value;
		if (!xml_element->QueryStringAttribute(name, &string_value) == tinyxml2::XML_SUCCESS)
		{
			//Only add error if it was non optional
			if (attribute_type == AttributeType::NonOptional)
			{
				AddError(load_context, "Error reading non optional attribute <%s> in node <%s>, line <%i>", name, load_context.name, xml_element->GetLineNum());
				return false;
			}
		}
		else
		{
			auto& conversion_table = ConversionTable<TYPE>::table;
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
			AddError(load_context, "Error converting value <%s> in attribute <%s> in node <%s>, line <%i>", string_value, name, load_context.name, xml_element->GetLineNum());
			return false;
		}
		return true;
	}
}
