//
// Created by ricky on 10/11/23.
//
#pragma once
#include <string>
#include <cstring>
#include <sys/mman.h>
#include "index/Index.hpp"
#include "meta.hpp"

namespace codes{

template<class Value, size_t B = 512>
class disk_map{
private:
	struct MetadataBlock {
		size_t M; // total number of strings
		size_t offset_to_heads; // offset to index string
		size_t n_blocks; // number of strings
	};

	uint8_t *data;
	uint8_t *compressed_blocks;
	size_t data_size;

	MetadataBlock *metadata_block; // pointer to block 0
	std::vector<std::pair<const char*, size_t>> index_string;

	// How many integers to read for a given datum
	static constexpr size_t N = [] {
		if constexpr (std::is_integral_v<Value>)
			return 1;

		if constexpr (is_std_array_v<Value>)
			return Value().size();


		return Value::serialize_size;
	}();

	static bool compare(const std::pair<const char*, size_t>& a, const std::pair<const char*, size_t>& b) {
		return std::strcmp(a.first, b.first) < 0;
	}

public:
	class iterator{
	private:
		disk_map& parent;
		uint8_t *offset_to_datum;
		uint8_t *offset_to_next_datum = nullptr;
		size_t index;
		size_t current_block;
		std::string current_key;
		Value current_value;

	private:
		void parse_value(uint8_t *&offset)
		{
			std::array<uint64_t, N> values;
			for(size_t i = 0; i < N; i++)
			{
				auto t = codes::VariableBytes::parse(offset);
				offset += t.second;
				values[i] = t.first;
			}

			// If integral
			if constexpr (std::is_integral_v<Value>)
				current_value = values[0];
			else if constexpr (std::is_array_v<Value>)
				for(size_t i = 0; i < N; i++)
					current_value[i] = values[i];
			else // Object
				current_value = Value::deserialize(values);
		}

		void parse(uint8_t *offset)
		{
			if((uint64_t)offset % B != 0)
			{
				auto t = codes::VariableBytes::parse(offset);
				size_t prefix_len = t.first;
				offset += t.second;

				parse_value(offset);

				auto postfix = std::string((char *) offset);
				current_key =
						std::string(parent.index_string[current_block].first).substr(0, prefix_len)
						+ postfix;

				offset += postfix.size() + 1;
			}
			else
			{
				// First block: b_i
				auto t = codes::VariableBytes::parse(offset);
				offset += t.second;

				parse_value(offset);

				current_block++;
				current_key = parent.index_string[current_block].first;
			}
			offset_to_next_datum = offset;

		}

	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type = std::pair<std::string, Value>;

		iterator(disk_map& p, uint8_t *datum, size_t index, size_t current_block):
			parent(p), offset_to_datum(datum), index(index), current_block(current_block)
		{
			if(index < parent.metadata_block->M)
				parse(offset_to_datum);
		}

		iterator& operator++()
		{
			offset_to_datum = offset_to_next_datum;

			// If next offset is zero, then EOB, align to next block
			if (*offset_to_next_datum == 0)
				offset_to_datum += B - ((uint64_t)offset_to_datum % B);

			parse(offset_to_next_datum);

			return *this;
		}

		const value_type& operator*() const
		{
			return {current_key, current_value};
		}

		bool operator==(const iterator& other) const
		{
			return index == other.index;
		}

		bool operator!=(const iterator& other) const
		{
			return index != other.index;
		}
	};

	iterator begin()
	{
		return iterator(*this, compressed_blocks, 0, -1);
	}
	iterator end()
	{
		return iterator(*this, compressed_blocks, metadata_block->M, metadata_block->n_blocks);
	}

	iterator find(const std::string& q)
	{
		// Perform binary search on the heads' blocks
		auto block_headers_it = std::lower_bound(
				index_string.begin(), index_string.end(),
				std::make_pair(q.c_str(), 0), compare);

		auto block_number = block_headers_it - index_string.begin();

		// Found it
		if(block_headers_it != index_string.end() and block_headers_it->first == q)
			return iterator(*this, compressed_blocks + block_number*B, block_headers_it->second, block_number);

		// Not valid result, move to the left
		if (block_headers_it == index_string.end() or block_headers_it->first != q)
		{
			--block_headers_it;
			--block_number;
		}

		// Now iterate all string 'til I see it
		iterator block_start_it(*this, compressed_blocks + block_number*B, block_headers_it->second, block_number);
		iterator block_end_it(*this, compressed_blocks + (block_number + 1)*B, (block_headers_it + 1)->second, block_number + 1);

		for(auto it = block_start_it; it != block_end_it; ++it)
			if(it->second == q)
				return it;

		return end();
	}

	Value at(const std::string& q)
	{
		return find(q)->second;
	}

	explicit disk_map(const std::string& file_name)
	{
		auto lexicon_minfo = sindex::mmap_helper(file_name.c_str());
		data = (uint8_t*)lexicon_minfo.first;
		data_size = lexicon_minfo.second;
		compressed_blocks = data + B;
		//metadata_block_ = reinterpret_cast<MetadataBlock*>(data);
		if (data_size < sizeof(MetadataBlock))
			abort();

		metadata_block = (MetadataBlock *)(data);
		index_string.reserve(metadata_block->n_blocks); // to reserve the space for the strings
		char* curr = (char*) data + metadata_block->offset_to_heads;
		index_string.emplace_back(curr, 0);

		while(index_string.size() < metadata_block->n_blocks){
			curr++;
			if(*curr != '\0')
				continue;

			auto b_i = codes::VariableBytes::parse(compressed_blocks + B * index_string.size());
			index_string.emplace_back(curr+1, b_i.first);
		}
	}

	~disk_map()
	{
		munmap(data, data_size);
	}


};

}
