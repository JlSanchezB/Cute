#ifndef BVH_H_
#define BVH_H_

#include <ext/glm/vec3.hpp>
#include "collision.h"
#include <algorithm>
#include <vector>

//Base to https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/

namespace helpers
{
	inline uint32_t ExpandBits(uint32_t value)
	{
		value = (value * 0x00010001u) & 0xFF0000FFu;
		value = (value * 0x00000101u) & 0x0F00F00Fu;
		value = (value * 0x00000011u) & 0xC30C30C3u;
		value = (value * 0x00000005u) & 0x49249249u;
		return value;
	}

	//Morton codes, [0,1] cube to 30bits
	inline uint32_t Morton(glm::vec3 position)
	{
		position.x = glm::clamp(position.x * 1024.f, 0.f, 1023.f);
		position.y = glm::clamp(position.y * 1024.f, 0.f, 1023.f);
		position.z = glm::clamp(position.z * 1024.f, 0.f, 1023.f);

		uint32_t xx = ExpandBits(static_cast<uint32_t>(position.x));
		uint32_t yy = ExpandBits(static_cast<uint32_t>(position.y));
		uint32_t zz = ExpandBits(static_cast<uint32_t>(position.z));

		return xx * 4 + yy * 2 + zz;
	}

	inline uint32_t CommonUpperBits(const uint32_t a, const uint32_t b)
	{
		//x86 intrisic
		return _tzcnt_u32(a ^ b);
	}

	//Basic linear BVH
	//Fast to build and to update (without rebuilding the structure), not the best BVH
	template<typename INSTANCE, typename SETTINGS>
	class LinearBVH
	{
	public:
		//Build the BVH from an instances array and a bounds
		void Build(const INSTANCE* const instances, uint32_t num_instances, const AABB& bounds);

		//Navigate the BVH and call the visitor with each instance that are inside the bounds
		template <typename VISITOR>
		void Visit(const AABB& bounds, VISITOR&& visitor);

	private:

		using IndexType = uint32_t;
		constexpr IndexType kInvalidIndex = static_cast<IndexType>(-1);
		
		//A node in the BVH
		//It can be an internal node or a leaf
		//Internal nodes has left and right node, left is the next node and right is next right_node
		//Leaf nodes have a range of instances
		struct Node
		{
			AABB bounds;
			bool leaf : 1;
			IndexType num_leafs : 31;
			union
			{
				IndexType right_node;
				IndexType leaf_offset;
			};
		}; //32 bytes

		//Sorted leafs
		std::vector<INSTANCE> m_leafs;
		//Parent node for each leaf, used when you want to update the parents bounds
		std::vector<IndexType> m_leafs_parents;
		//List of nodes, node zero is the root
		std::vector<Node> m_nodes;
		//Parent for each node, used to update the parent bounds
		std::vector<IndexType> m_node_parents;

		//Struct used for building the BVH
		struct InstanceInfo
		{
			INSTANCE instance;
			AABB aabb;
			uint32_t morton_codes;
		};

		//Recursive node building
		AABB NodeBuild(const IndexType parent_index, InstanceInfo* instance_info, const IndexType instance_offset, const IndexType instance_count);

	};

	template<typename INSTANCE, typename SETTINGS>
	inline void LinearBVH<INSTANCE, SETTINGS>::Build(const INSTANCE * const instances, const uint32_t num_instances, const AABB& bounds)
	{
		//Collect all the bbox and calculate the morton codes
		std::vector<InstanceInfo> instances_info;
		instances_info.reserve(num_instances);
		for (auto& instance : instances)
		{
			//Get AABB
			AABB aabb = SETTINGS::GetAABB(instance);
			glm::vec3 center = (aabb.min + aabb.max) / 2.f;

			//Move it to 0-1 range
			glm::vec3 cube_center = (center - bounds.min) / (bounds.max - bounds.min);

			instances_info.emplace_back({ instance, aabb, Morton(cube_center) });
		}

		//Sort leaf infos
		std::sort(instances_info.begin(), instances_info.end(), [](const LeafInfo& a, const LeafInfo& b)
			{
				return a.morton < b.morton;
			});

		//Reserve space for leafs
		m_leafs.reserve(num_instances);
		m_leafs_parents.reserve(num_instances);

		//Build BVH, the first is the root
		NodeBuild(kInvalidIndex, instance_info, 0, num_instances);

		check(m_leafs.size() == num_instances);
	}

	template<typename INSTANCE, typename SETTINGS>
	inline AABB LinearBVH<INSTANCE, SETTINGS>::NodeBuild(const IndexType parent_index, InstanceInfo* instance_info, const IndexType instance_offset, const IndexType instance_count)
	{
		//Reserve a node
		uint32_t node_index = m_nodes.size();
		m_nodes.push_back();
		m_node_parents.push_back(parent_index);
		Node& node = m_nodes.back();

		//Calculate the bbox for the node
		//Calculate the split instance
		uint32_t max_common_upper_bits = 0;
		uint32_t top_morton_code = 0;
		IndexType split_index = kInvalidIndex;
		for (IndexType i = instance_offset; i < (instance_offset + instance_count); ++i)
		{
			InstanceInfo& instance_info = instance_info[i];

			uint32_t common_upper_bits = CommonUpperBits(instance_info.morton_codes, top_morton_code);
			if (common_upper_bits > max_common_upper_bits)
			{
				top_morton_code = instance_info.morton_codes;
				max_common_upper_bits = common_upper_bits;
				split_index = i;
			}
		}
		//If leaf
		if (max_common_upper_bits == 0)
		{
			//Is a leaf
			node.leaf = true;
			node.leaf_offset = m_leafs.size();
			node.num_leafs = instance_count;
			for (IndexType i = instance_offset; i < (instance_offset + instance_count); ++i)
			{
				InstanceInfo& instance_info = instance_info[i];
				//Calculate the bbox of this leaf node
				node.bounds.Add(instance_info.aabb);

				//Set the leaf index
				SETTINGS::SetLeafIndex(m_leafs.size());

				//Push the leaf
				m_leafs.push_back(instance_info[i].instance);
				m_leafs_parents.push_back(node_index);
			}
		}
		else
		{
			//Node
			node.leaf = false;

			//Create first node and get the aabb
			node.bounds = NodeBuild(node_index, instance_info, instace_offset, split_index - instace_offset + 1);

			//Create second node and add the aabb
			node.right_node = m_nodes.size();

			node.bounds.Add(NodeBuild(node_index, instance_info, split_index, instance_count - (split_index - instace_offset + 1)));
		}

		return node.bounds;
	}
	template<typename INSTANCE, typename SETTINGS>
	template<typename VISITOR>
	inline void LinearBVH<INSTANCE, SETTINGS>::Visit(const AABB& bounds, VISITOR&& visitor)
	{
		//Start with the root
		std::vector<IndexType> node_stack = {0};

		while (!node_stack.empty())
		{
			//Get a the node index and pop
			IndexType node_index = node_stack.back();
			node_stack.pop_back();

			const Node& node = m_nodes[node_index];

			if (CollisionAABBVsAABB(bounds, node.bounds))
			{
				if (node.leaf)
				{
					//visit
					for (IndexType i = instance_offset; i < (instance_offset + instance_count); ++i)
					{
						visitor(m_leafs[i]);
					}
				}
				else
				{
					//Push left and right
					node_stack.push_back(node_index + 1);
					node_stack.push_back(node.right_node);
				}
			}
		}

	}
}
#endif //BVH_H_