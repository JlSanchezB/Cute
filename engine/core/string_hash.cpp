#include "string_hash.h"
#include <unordered_map>
#include <string>
#include "log.h"

namespace core
{
#ifdef _STRING_HASH_MAP_ENABLED_
	//String hash maps for finding the string value from a hash, only working in debug
	struct NamespaceStringHashMap
	{
		std::unordered_map<uint16_t, std::string> string_hash_map_16;
		std::unordered_map<uint32_t, std::string> string_hash_map_32;
		std::unordered_map<uint64_t, std::string> string_hash_map_64;
		
		template<typename TYPE>
		auto& GetStringHashMap();

		template<>
		auto& GetStringHashMap<uint16_t>() { return string_hash_map_16;};

		template<>
		auto& GetStringHashMap<uint32_t>() { return string_hash_map_32; };

		template<>
		auto& GetStringHashMap<uint64_t>() { return string_hash_map_64; };
	};

	//Global map for each namespace
	std::unordered_map<uint64_t, NamespaceStringHashMap>* g_namespaces_string_hash_table = nullptr;

	void CreateStringHashMap()
	{
		g_namespaces_string_hash_table = new std::unordered_map<uint64_t, NamespaceStringHashMap>;
	}
	void DestroyStringHashMap()
	{
		delete g_namespaces_string_hash_table;
		g_namespaces_string_hash_table = nullptr;
	}

	//Debug access funtions
	template<typename TYPE>
	const char* GetStringFromHashT(uint64_t namespace_hash, TYPE string_hash)
	{
		if (g_namespaces_string_hash_table)
		{
			auto namespace_find = g_namespaces_string_hash_table->find(namespace_hash);
			if (namespace_find != g_namespaces_string_hash_table->end())
			{
				auto& namespace_string_hash_map = namespace_find->second;

				auto& string_hash_find = namespace_string_hash_map.GetStringHashMap<TYPE>().find(string_hash);
				if (string_hash_find != namespace_string_hash_map.GetStringHashMap<TYPE>().end())
				{
					return string_hash_find->second.c_str();
				}
				else
				{
					return "String Hash invalid";
				}
			}
			else
			{
				return "Namespace invalid";
			}
		}
		else
		{
			return "StringHashMap not created";
		}
	}

	const char* GetStringFromHash(uint64_t namespace_hash, uint64_t string_hash)
	{
		return GetStringFromHashT(namespace_hash, string_hash);
	}

	const char* GetStringFromHash(uint64_t namespace_hash, uint32_t string_hash)
	{
		return GetStringFromHashT(namespace_hash, string_hash);
	}

	const char* GetStringFromHash(uint64_t namespace_hash, uint16_t string_hash)
	{
		return GetStringFromHashT(namespace_hash, string_hash);
	}

	template<typename TYPE>
	void AddStringHashT(uint64_t namespace_hash, TYPE string_hash, const char* string)
	{
		if (g_namespaces_string_hash_table)
		{
			auto& namespace_string_hash_map = (*g_namespaces_string_hash_table)[namespace_hash];

			auto string_hash_find = namespace_string_hash_map.GetStringHashMap<TYPE>().find(string_hash);
			if (string_hash_find != namespace_string_hash_map.GetStringHashMap<TYPE>().end())
			{
				//Check if the hash is the same
				if (string_hash_find->second == string)
				{
					//Same string, all correct
				}
				else
				{
					//Different string, that can not happen
					//Collision detected, fatal error
					core::LogError("Collision detected in string hashes, same hash <%i> in two values<'%s','%s'>", string_hash, string, string_hash_find->second.c_str());
					throw std::runtime_error("Collision detected in string hashes");
				}
			}
			else
			{
				//Add the hash, all correct
				namespace_string_hash_map.GetStringHashMap<TYPE>()[string_hash] = string;
			}
		}
	}

	void AddStringHash(uint64_t namespace_hash, uint64_t string_hash, const char* string)
	{
		AddStringHashT(namespace_hash, string_hash, string);
	}

	void AddStringHash(uint64_t namespace_hash, uint32_t string_hash, const char* string)
	{
		AddStringHashT(namespace_hash, string_hash, string);
	}

	void AddStringHash(uint64_t namespace_hash, uint16_t string_hash, const char* string)
	{
		AddStringHashT(namespace_hash, string_hash, string);
	}
#endif
}