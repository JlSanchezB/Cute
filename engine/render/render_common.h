#include <stdarg.h>

namespace
{
	template <typename T, std::size_t N>
	constexpr std::size_t countof(T const (&)[N]) noexcept
	{
		return N;
	}

	inline void AddError(render::LoadContext& load_context, const char* message, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, message);
		vsnprintf_s(buffer, 1024, 1024 - 1, message, args);
		va_end(args);

		load_context.errors.push_back(buffer);
	}

	inline bool QuerySizeAttribute(tinyxml2::XMLElement* xml_element, const char* name, size_t* value)
	{
		int64_t int64_value;
		if (xml_element->QueryInt64Attribute(name, &int64_value) == tinyxml2::XML_SUCCESS)
		{
			*value = static_cast<size_t>(int64_value);
			return true;
		}
		else
		{
			return false;
		}
	}

	template <typename ENUM, typename CONVERSION_TABLE>
	inline bool QueryEnumAttribute(tinyxml2::XMLElement* xml_element, const char* name, ENUM* value, CONVERSION_TABLE& conversion_table)
	{
		const char* string_value;
		if (xml_element->QueryStringAttribute(name, &string_value) == tinyxml2::XML_SUCCESS)
		{
			//Check the conversion table for it
			for (size_t i = 0; i <std::size(conversion_table); ++i)
			{
				auto& pair = conversion_table[i];
				if (strcmp(pair.first, string_value) == 0)
				{
					//found
					*value = pair.second;
					return true;
				}
			}
			return false;
		}
		else
		{
			return false;
		}
	}
}
