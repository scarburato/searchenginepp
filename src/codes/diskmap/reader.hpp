#pragma once
#include <algorithm>
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <cassert>
#include <vector>
#include "../../util/memory.hpp"
#include "../../meta.hpp"
#include "../variable_blocks.hpp"
#include "diskmap.hpp"


namespace codes
{

/**
 * Disk-based map reader
 * @tparam Value type of the values
 * @tparam B block size
 */
template<class Value, size_t B = BLOCK_SIZE>
class disk_map{
private:
	struct MetadataBlock {
		size_t M; // total number of strings
		size_t offset_to_heads; // offset to index string
		size_t n_blocks; // number of strings
	}  __attribute__((__packed__));

	uint8_t *raw_data;
	uint8_t *cblocks_base;
	size_t data_size;

	MetadataBlock *metadata; // pointer to block 0
	std::vector<std::pair<const char*, size_t>> index_string;

	// How many integers to read for a given datum
	static constexpr size_t N = [] {
		if constexpr (std::is_integral_v<Value>)
			return 1;
		else if constexpr (is_std_array_v<Value>)
			return Value().size();
		else
			return Value::serialize_size;
	}();

	/**
	 * Compares two pairs of strings
	 * @param a first pair
	 * @param b second pair
	 * @return true if a < b, false otherwise
	 */
	static bool compare(const std::pair<const char*, size_t>& a, const std::pair<const char*, size_t>& b) {
		return std::strcmp(a.first, b.first) < 0;
	}

public:
	class iterator{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type = std::pair<std::string, Value>;

	private:
		disk_map& parent;
		size_t offset_to_datum;
		size_t offset_to_next_datum = 0;
		size_t index;
		size_t current_block;

		// <current_key, current_value>
		value_type current;

		/**
		 * Parses a datum's value and moves offset to end of the encoded value
		 */
		void parse_value(size_t& offset)
		{
			// Create values as an array or vector based on the value of N
        	std::conditional_t<(N == 0), std::vector<uint64_t>, std::array<uint64_t, N>> values;

			size_t numbers;
			// If N is equal to 0, we are reading a variable number of integers
			if constexpr (N != 0)
				numbers = N;
			else
			{
				auto compressed_size = codes::VariableBytes::parse(parent.cblocks_base + offset);
				numbers = compressed_size.first;
				offset += compressed_size.second;
			}

			// Read all integers
			for(size_t i = 0; i < numbers; i++)
			{
				auto t = codes::VariableBytes::parse(parent.cblocks_base + offset);
				offset += t.second;
				if constexpr (N != 0)
					values[i] = t.first;
				else
					values.push_back(t.first);
			}

			// Assign values to current.second
			if constexpr (std::is_integral_v<Value>)
				current.second = values[0];
			else if constexpr (is_std_array_v<Value>)
				for(size_t i = 0; i < N; i++)
					current.second[i] = values[i];
			else
				current.second = Value::deserialize(values);
		}

		/**
		 * Parses one datum from the block and computes the offset to next one
		 * This method as collateral effects as it updates the iterator's data
		 * @param offset where to parse
		 */
		void parse(size_t offset)
		{
			// Two cases: not first element of block
			if(offset % B != 0)
			{
				// First element of non-block start: the common prefix's length
				size_t prefix_len = *(parent.cblocks_base + offset++);

				// Then we can compute the complete key string and save it
				auto postfix = std::string((char *)parent.cblocks_base + offset);
				auto new_str =
						std::string(parent.index_string[current_block].first).substr(0, prefix_len)
						+ postfix;

				assert(new_str > current.first and new_str.size() < 255 and not current.first.empty());
				current.first = std::move(new_str);

				// Move offset to the end of the string
				offset += postfix.size() + 1;

				// Finally a sequence of values
				parse_value(offset);
			}
			else // First element of block
			{
				// First block: b_i
				// First element of a block is the encoded index of the block's head. We
				// don't really make use of it, but we have to move forward nonetheless
				auto t = codes::VariableBytes::parse(parent.cblocks_base + offset);
				offset += t.second;

				// Then a sequence of values associated to the block-head's key
				parse_value(offset);

				// Current_key is retrieved from the vector
				current_block++;
				current.first = parent.index_string[current_block].first;

				// For debug release: check if indices are still aligned
				assert(t.first == index);
				assert(t.first == parent.index_string[current_block].second);
			}

			// Determine if the current offset marks the end of a block and calculates the offset to the next datum accordingly. 
			// If the offset is not at the end of a block, it checks if the next offset indicates the end of a block and adjusts 
			// the offset accordingly.
			if(offset % B != 0 and (
					(offset + 1) % B == 0 or
					(offset + 2) % B == 0 or 
					(*(parent.cblocks_base + offset) == 0x00 and *(parent.cblocks_base + offset + 1) == 0x00))
					)
				offset_to_next_datum = offset + (B - (offset % B));
			else
				offset_to_next_datum = offset;
		}

	public:
		iterator(disk_map& p, size_t off, size_t index, size_t current_block_):
				parent(p), offset_to_datum(off), index(index), current_block(current_block_)
		{
			if(index < parent.metadata->M)
			{
				current_block--; // bc parse() will increase it again... ops...
				parse(offset_to_datum);
			}
		}

		iterator& operator++()
		{
			offset_to_datum = offset_to_next_datum;
			++index;

			if(index < parent.metadata->M)
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
			return index == other.index;
		}

		bool operator!=(const iterator& other) const {return not operator==(other);}

		size_t memory_offset() const {return offset_to_datum;}
	};

	iterator begin()
	{
		return iterator(*this, 0, 0, 0);
	}
	iterator end()
	{
		return iterator(*this, B*metadata->n_blocks, metadata->M, metadata->n_blocks);
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
			return iterator(*this, block_number * B, block_headers_it->second, block_number);

		// Not valid result, move to the left
		if (block_headers_it == index_string.end() or block_headers_it->first != q)
		{
			--block_headers_it;
			--block_number;
		}

		// Now iterate all string 'til I see it
		iterator block_start_it(*this, block_number * B, block_headers_it->second, block_number);

		// Notice how we check if the next block is valid, if it's not we set the end of the block to M,
		// that is an invalid index, this avoids to deference/parse invalid data
		const auto block_end_start_index = block_headers_it + 1 == index_string.end() ? metadata->M : (block_headers_it + 1)->second;
		iterator block_end_it(*this, (block_number + 1) * B, block_end_start_index, block_number + 1);

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

	size_t size() const {return metadata->M;}

	/**
	 * Constructor
	 * @param memory memory area
	 */
	explicit disk_map(memory_area& memory)
	{
		auto lexicon_minfo = memory.get();
		raw_data = lexicon_minfo.first;
		data_size = lexicon_minfo.second;

		cblocks_base = raw_data + B;

		metadata = (MetadataBlock *)(raw_data);
		index_string.reserve(metadata->n_blocks); // to reserve the space for the strings
		char* curr = (char*)raw_data + metadata->offset_to_heads;
		index_string.emplace_back(curr, 0);

		// load all blocks' heads' pointers in memory
		while(index_string.size() < metadata->n_blocks){
			curr++;
			if(*curr != '\0')
				continue;

			auto b_i = codes::VariableBytes::parse(cblocks_base + B * index_string.size());
			index_string.emplace_back(curr+1, b_i.first);
		}
	}

};

}
