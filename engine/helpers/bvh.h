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
		using IndexType = SETTINGS::template IndexType;

		//Build the BVH from an instances array and a bounds
		void Build(SETTINGS* settings, INSTANCE* const instances, uint32_t num_instances, const AABB& bounds);

		//Navigate the BVH and call the visitor with each instance that are inside the bounds
		template <typename VISITOR>
		void Visit(const AABB& bounds, VISITOR&& visitor) const;

	private:
		constexpr static IndexType kInvalidIndex = static_cast<IndexType>(-1);
		
		//A node in the BVH
		//It can be an internal node or a leaf
		//Internal nodes has left and right node, left is the next node and right is next right_node
		//Leaf nodes have a range of instances
		struct Node
		{
			AABB bounds;
			bool leaf : 1;
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

		//Max depth
		uint32_t m_max_depth;

		//Struct used for building the BVH
		struct InstanceInfo
		{
			INSTANCE instance;
			AABB aabb;
			uint32_t morton_codes;

			InstanceInfo(const INSTANCE& _instance, const AABB& _aabb, const uint32_t _morton_codes) :
				instance(_instance), aabb(_aabb), morton_codes(_morton_codes)
			{
			}
		};

		//Recursive node building
		AABB NodeBuild(SETTINGS* settings, const IndexType parent_index, std::vector<InstanceInfo>& instances_info, IndexType& next_node_index, IndexType& next_leaf_index, const IndexType instance_first, const IndexType instance_last);

	};

	template<typename INSTANCE, typename SETTINGS>
	inline void LinearBVH<INSTANCE, SETTINGS>::Build(SETTINGS* settings, INSTANCE * const instances, const uint32_t num_instances, const AABB& bounds)
	{
		m_leafs.clear();
		m_leafs_parents.clear();
		m_nodes.clear();
		m_node_parents.clear();

		//Collect all the bbox and calculate the morton codes
		std::vector<InstanceInfo> instances_info;
		instances_info.reserve(num_instances);
		for (uint32_t i = 0; i < num_instances; ++i)
		{
			const INSTANCE& instance = instances[i];
			//Get AABB
			AABB aabb = settings->GetAABB(instance);
			glm::vec3 center = (aabb.min + aabb.max) / 2.f;

			//Move it to 0-1 range
			glm::vec3 cube_center = (center - bounds.min) / (bounds.max - bounds.min);

			instances_info.emplace_back(instance, aabb, Morton(cube_center));
		}

		//Sort leaf infos
		std::sort(instances_info.begin(), instances_info.end(), [](const InstanceInfo& a, const InstanceInfo& b)
			{
				return a.morton_codes < b.morton_codes;
			});

		//Resize space for leafs
		m_leafs.resize(num_instances);
		m_leafs_parents.resize(num_instances);

		//Reserve space for nodes, it is a binary tree, in the perfect distribution will be twice nodes than leafs
		//We are going to do a fake node after the root to improve the cache, so 2 * x - 1 + 1
		m_nodes.resize(num_instances * 2);
		m_node_parents.resize(num_instances * 2);

		IndexType next_node_index = 0;
		IndexType next_leaf_index = 0;

		//Build BVH, the first is the root
		NodeBuild(settings, kInvalidIndex, instances_info, next_node_index, next_leaf_index, 0, num_instances - 1);

		assert(next_leaf_index == num_instances);
		assert(next_node_index == num_instances * 2);

		m_max_depth = static_cast<uint32_t>(log2f(static_cast<float>(num_instances)));
	}

	template<typename INSTANCE, typename SETTINGS>
	inline AABB LinearBVH<INSTANCE, SETTINGS>::NodeBuild(SETTINGS* settings, const IndexType parent_index, std::vector<InstanceInfo>& instances_info, IndexType& next_node_index, IndexType& next_leaf_index, const IndexType instance_first, const IndexType instance_last)
	{
		//Reserve a node
		IndexType node_index = next_node_index++;
		
		m_node_parents[node_index] = parent_index;

		//Fake node for better cache after the root node
		if (node_index == 0)
		{
			next_node_index++; 
		}

		//local copy of the node
		Node& node = m_nodes[node_index];

		assert(instance_first <= instance_last);

		if (instance_first == instance_last)
		{
			//It is a LEAF
			node.leaf = true;

			InstanceInfo& instance_info = instances_info[instance_first];

			//Allocate a leaf
			IndexType leaf_index = next_leaf_index++;

			node.bounds = instance_info.aabb;
			node.leaf_offset = leaf_index;

			//Set the leaf index
			settings->SetLeafIndex(instance_info.instance, leaf_index);

			//Push the leaf
			m_leafs[leaf_index] = instance_info.instance;
			m_leafs_parents[leaf_index] = node_index;
		}
		else
		{
			//It is a node

			//Calculate the bbox for the node
			//Calculate the split instance

			uint32_t first_morton_code = instances_info[instance_first].morton_codes;
			uint32_t last_morton_code = instances_info[instance_last].morton_codes;
			IndexType split_index = instance_first;

			if (first_morton_code == last_morton_code)
			{
				//Same morton code, split in half
				split_index = (instance_first + instance_last) / 2;
			}
			else
			{
				uint32_t step = instance_last - instance_first;
				//Start with the first one
				uint32_t max_prefix = CommonUpperBits(first_morton_code, last_morton_code);

				//Look for the max common upper bits that shares with the first one
				//Binary search
				do
				{
					step = (step + 1) >> 1;
					uint32_t new_split = split_index + step;

					if (new_split < instance_last)
					{
						uint32_t split_prefix = CommonUpperBits(first_morton_code, instances_info[new_split].morton_codes);
						if (split_prefix > max_prefix)
						{
							//New split
							split_index = new_split;
						}
					}
				} while (step > 1);
			}

			//Create the two child nodes
			node.leaf = false;

			//Create first node and get the aabb
			node.bounds = NodeBuild(settings, node_index, instances_info, next_node_index, next_leaf_index, instance_first, split_index);

			//Create second node and add the aabb
			node.right_node = next_node_index;
			node.bounds.Add(NodeBuild(settings, node_index, instances_info, next_node_index, next_leaf_index, split_index + 1, instance_last));

		}
		return node.bounds;
	}
	template<typename INSTANCE, typename SETTINGS>
	template<typename VISITOR>
	inline void LinearBVH<INSTANCE, SETTINGS>::Visit(const AABB& bounds, VISITOR&& visitor) const
	{
		assert(!m_leafs.empty());
		assert(!m_nodes.empty());

		//Start with the root
		std::vector<IndexType> node_stack = {0};
		node_stack.reserve(m_max_depth);

		while (!node_stack.empty())
		{
			assert(node_stack.size() < m_nodes.size());

			//Get a the node index and pop
			IndexType node_index = node_stack.back();
			node_stack.pop_back();

			const Node& node = m_nodes[node_index];

			if (CollisionAABBVsAABB(bounds, node.bounds))
			{
				if (node.leaf)
				{
					//visit
					visitor(m_leafs[node.leaf_offset]);
				}
				else
				{
					//Push left and right
					if (node_index == 0) node_index++; //Fake node for cache aligment in the root node

					node_stack.push_back(node_index + 1);
					node_stack.push_back(node.right_node);
				}
			}
		}

	}
}
#endif //BVH_H_