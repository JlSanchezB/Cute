#include "string_hash.h"
#include <unordered_map>
#include <string>
#include <stdexcept>
#include "log.h"
#include "fast_map.h"
#include "virtual_buffer.h"

namespace core
{
#ifdef _STRING_HASH_MAP_ENABLED_
	//String hash maps for finding the string value from a hash, only working in debug
	struct NamespaceStringHashMap
	{
		core::FastMap<uint16_t, const char*> string_hash_map_16;
		core::FastMap<uint32_t, const char*> string_hash_map_32;
		core::FastMap<uint64_t, const char*> string_hash_map_64;

		template<typename TYPE>
		auto& GetStringHashMap();

		template<>
		auto& GetStringHashMap<uint16_t>(){ return string_hash_map_16;};

		template<>
		auto& GetStringHashMap<uint32_t>(){ return string_hash_map_32; };

		template<>
		auto& GetStringHashMap<uint64_t>(){ return string_hash_map_64; };
	};

	//Global map for each namespace
	core::FastMap<uint64_t, NamespaceStringHashMap>* g_namespaces_string_hash_table = nullptr;

	core::VirtualBufferInitied<1024 * 1024>* g_string_buffer;

	const char* AddString(const char* string)
	{
		if (g_string_buffer == nullptr)
		{
			g_string_buffer = new core::VirtualBufferInitied<1024 * 1024>;
		}
		//Increase the size of the buffer by the new string
		size_t string_size = strlen(string);
		char* buffer = (reinterpret_cast<char*>(g_string_buffer->GetPtr())) + g_string_buffer->GetCommitedSize();
		g_string_buffer->SetCommitedSize(g_string_buffer->GetCommitedSize() + string_size + 1);

		//Copy the string
		strcpy_s(buffer, string_size + 1, string);

		return buffer;
	}

	void DestroyStringHashMap()
	{
		delete g_namespaces_string_hash_table;
		g_namespaces_string_hash_table = nullptr;

		delete g_string_buffer;
		g_string_buffer = nullptr;
	}

	//Debug access funtions
	template<typename TYPE>
	const char* GetStringFromHashT(uint64_t namespace_hash, TYPE string_hash)
	{
		if (g_namespaces_string_hash_table)
		{
			auto& namespace_find = g_namespaces_string_hash_table->Find(namespace_hash);
			if (namespace_find)
			{
				auto string_hash_find = namespace_find->GetStringHashMap<TYPE>().Find(string_hash);
				if (string_hash_find)
				{
					return *string_hash_find;
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
		if (g_namespaces_string_hash_table == nullptr)
		{
			g_namespaces_string_hash_table = new core::FastMap<uint64_t, NamespaceStringHashMap>;
		}

		auto namespace_string_hash_map = g_namespaces_string_hash_table->Find(namespace_hash);

		if (!namespace_string_hash_map)
		{
			//Create namespace
			namespace_string_hash_map = g_namespaces_string_hash_table->Insert(namespace_hash);
		}

		auto string_hash_find = namespace_string_hash_map->GetStringHashMap<TYPE>().Find(string_hash);
		if (string_hash_find)
		{
			//Check if the hash is the same
			if (strcmp(*string_hash_find, string) == 0)
			{
				//Same string, all correct
			}
			else
			{
				//Different string, that can not happen
				//Collision detected, fatal error
				core::LogError("Collision detected in string hashes, same hash <%i> in two values<'%s','%s'>", string_hash, string, *string_hash_find);
				throw std::runtime_error("Collision detected in string hashes");
			}
		}
		else
		{
			//Add string to the string table


			//Add the hash, all correct
			namespace_string_hash_map->GetStringHashMap<TYPE>().Insert(string_hash, AddString(string));
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