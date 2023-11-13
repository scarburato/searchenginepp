//
// Created by ricky on 10/11/23.
//
#pragma once
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <cassert>
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

		if constexpr (std::is_class_v<Value>)
			return Value::serialize_size;
	}();

	static bool compare(const std::pair<const char*, size_t>& a, const std::pair<const char*, size_t>& b) {
		return std::strcmp(a.first, b.first) < 0;
	}

public:
	class iterator{
	public:
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type = std::pair<std::string, Value>;

	private:
		disk_map& parent;
		uint8_t *offset_to_datum;
		uint8_t *offset_to_next_datum = nullptr;
		size_t index;
		size_t current_block;

		// <current_key, current_value>
		value_type current;

		/**
		 * Parses a datum's value and moves offset to end of the encoded value
		 */
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
				current.second = values[0];
			else if constexpr (std::is_array_v<Value>)
				for(size_t i = 0; i < N; i++)
					current.second[i] = values[i];
			else // Object
				current.second = Value::deserialize(values);
		}

		/**
		 * Parses one datum from the block and computes the offset to next one
		 * This method as collateral effects as it updates the iterator's data
		 * @param offset where to parse
		 */
		void parse(uint8_t *offset)
		{
			// two cases: not first element of block
			if((uint64_t)offset % B != 0)
			{
				// First element of non-block start: the common prefix's length
				auto t = codes::VariableBytes::parse(offset);
				size_t prefix_len = t.first;
				offset += t.second;

				// then we can compute the complete key string
				auto postfix = std::string((char *) offset);
				current.first =
						std::string(parent.index_string[current_block].first).substr(0, prefix_len)
						+ postfix;

				offset += postfix.size() + 1;

				// Finally a sequence of values
				parse_value(offset);
			}
			else // first element of block
			{
				// First block: b_i
				// First element of a block is the encoded index of the block's head. We
				// don't really make use of it, but we have to move forward nonetheless
				auto t = codes::VariableBytes::parse(offset);
				offset += t.second;

				// then a sequence of values associated to the block-head's key
				parse_value(offset);

				// current_key is retrieved from the vector
				current_block++;
				current.first = parent.index_string[current_block].first;

				// For debug release: check if indexes are still aligned
				assert(t.first == index);
				assert(t.first == parent.index_string[current_block].second);
			}

			// If next o(s_b_j, s_(i+1)) == 0 and P(i+1) = '\0'
			// then block finished early. We must align ourselves to next
			if((uint64_t)offset + 1 % B != 0 and ((uint64_t)offset + 2 % B != 0 or (*offset == 0x00 and *(offset + 1) == 0x00)))
				offset_to_next_datum = offset + (B - ((uint64_t)offset % B));
			else
				offset_to_next_datum = offset;
		}

	public:

		iterator(disk_map& p, uint8_t *datum, size_t index, size_t current_block_):
				parent(p), offset_to_datum(datum), index(index), current_block(current_block_)
		{
			if(index < parent.metadata_block->M)
			{
				current_block--; // bc parse() will increase it again... ops...
				parse(offset_to_datum);
			}
		}

		iterator& operator++()
		{
			offset_to_datum = offset_to_next_datum;
			++index;

			parse(offset_to_next_datum);

			return *this;
		}

		const value_type& operator*() const
		{
			return current;
		}

		const value_type* operator->() const {return &current;}

		bool operator==(const iterator& other) const
		{
			assert(offset_to_datum == other.offset_to_datum);
			return index == other.index;
		}

		bool operator!=(const iterator& other) const {return not operator==(other);}
	};

	iterator begin()
	{
		return iterator(*this, compressed_blocks, 0, 0);
	}
	iterator end()
	{
		return iterator(*this, compressed_blocks, metadata_block->M, metadata_block->n_blocks);
	}

	/**
	 * Finds element
	 * @param q query
	 * @return the element if there's a match, end() otherwise
	 */
	iterator find(const std::string& q)
	{
		// Perform binary search on the blocks' heads
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

		// We may as well stop earlier if it->second > q
		for(auto it = block_start_it; it != block_end_it and it->first <= q; ++it)
			if(it->first == q)
				return it;

		// We found nothing
		return end();
	}

	Value at(const std::string& q)
	{
		return find(q)->second;
	}

	explicit disk_map(const memory_area& memory)
	{
		auto lexicon_minfo = memory.get();
		data = (uint8_t*)lexicon_minfo.first;
		data_size = lexicon_minfo.second;
		compressed_blocks = data + B;

		metadata_block = (MetadataBlock *)(data);
		index_string.reserve(metadata_block->n_blocks); // to reserve the space for the strings
		char* curr = (char*) data + metadata_block->offset_to_heads;
		index_string.emplace_back(curr, 0);

		// load all blocks' heads' pointers in memory
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
