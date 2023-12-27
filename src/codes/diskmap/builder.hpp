#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <ostream>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>
#include <list>
#include "../variable_blocks.hpp"
#include "diskmap.hpp"

namespace codes
{
/*
 *   disk_map_writer class for writing key-value pairs to disk efficiently
 * - Handles writing data blocks to a stream, managing metadata, and finalizing the storage structure.
 * - Utilizes variable compression techniques to optimize storage space.
 */
template<class Value, size_t B = BLOCK_SIZE>
class disk_map_writer
{
private:
    std::ostream& teletype; // Output stream
    off_t metadata_block_off; // Offset for metadata block
    std::vector<std::string> heads; // vector storing keys
    size_t current_bytes = 0; // Current bytes written
	uint64_t n_strings = 0; // total string written
	std::string debug_last_string; // Debug information for the last string processed

	// Constants and functions needed within the class
    static constexpr size_t N = [] {
		// Determine N based on the type of 'Value'
		if constexpr (std::is_integral_v<Value>)
			return 1;
		else if constexpr (is_std_array_v<Value>)
			return Value().size();
		else
			return Value::serialize_size;
	}();

	// Function to compute the common prefix between two strings
    static size_t compute_common_prefix(const char *a, const char *b)
    {
        size_t common_len = 0;
        for(; a[common_len] and b[common_len] and a[common_len] == b[common_len]; ++common_len)
            continue;

        return common_len;
    }
	// Function to align stream to the block size
    static void align_stream_to_block(std::ostream& ostr)
    {
        size_t next_block_off = B - ostr.tellp() % B;
        if(next_block_off != B)
            ostr.seekp(next_block_off, std::ios_base::cur);
    }
	// Function to create a new block and write data to it
    template<class Container>
    void new_block(const std::string& key, Container& compressed_values, size_t cvals_size)
    {
        heads.push_back(key); // add key to heads
        align_stream_to_block(teletype); // Align stream to block size

        auto bi_encoded = codes::VariableBytes(n_strings); // encode the number to strings

        teletype.write((char*)bi_encoded.bytes, bi_encoded.used_bytes); // write ecoded bytes

        for(auto cValue : compressed_values)
            teletype.write((char*)cValue.bytes, cValue.used_bytes); // write compressed values
        
        current_bytes = bi_encoded.used_bytes + cvals_size; // Update current bytes
    }

public:
	// constructor
	disk_map_writer() = delete;
	// Constructor initializes the disk_map_writer object with the output stream and metadata block offset.
    explicit disk_map_writer(std::ostream& teletype):
        teletype(teletype)
    {
        metadata_block_off = teletype.tellp(); // get offset for metadata block
        teletype.seekp(B, std::ios_base::cur); // move stream position
    }
	// Function to add a key-value pair
    void add(const std::pair<std::string, Value>& p) {add(p.first, p.second);}

	// Adds a key-value pair to the disk map:
	// - Compresses values and manages block creation based on keys and their sizes.
	// - Handles block creation, update, and size calculations.
    void add(const std::string& key, const Value& value)
    {
		assert(not key.empty() and key > debug_last_string and key.size() < 255);
		debug_last_string = key; // Update debug information

		// Create compressed_values as an array or vector based on the value of N
        std::conditional_t<(N == 0), std::vector<codes::VariableBytes>, std::array<codes::VariableBytes, N>> compressed_values;
        if constexpr (N != 0)
        {
			// generate compressed values based on Value type
            if constexpr (std::is_integral_v<Value>)
                compressed_values[0] = codes::VariableBytes(value);
            else if constexpr (is_std_array_v<Value>)
                for (size_t i = 0; i < N; i++)
                    compressed_values[i] = codes::VariableBytes(value[i]);
            else
            {
                auto values = value.serialize();
                for(size_t i = 0; i < N; i++)
                    compressed_values[i] = codes::VariableBytes(values[i]);
            }
        } 
        else 
        {
            auto values = value.serialize();
            compressed_values.push_back(codes::VariableBytes(values.size()));
            for (size_t i = 0; i < values.size(); i++)
                compressed_values.push_back(codes::VariableBytes(values[i]));
        }
        
        // Sum of total used bytes of all the values
        size_t total_used_bytes = std::accumulate(
				compressed_values.begin(), compressed_values.end(), 0,
				[](size_t sum, const auto& cValue) { return sum + cValue.used_bytes; });

		// If it's the first call to add()
        if(heads.empty())
        {
			new_block(key, compressed_values, total_used_bytes); // create a new block
			n_strings += 1; // increment total strings
            return;
        }

        size_t key_len = key.size() + 1;
        uint8_t common_len = compute_common_prefix(key.c_str(), heads.back().c_str());
        size_t diff_len = key_len - common_len;

        if(current_bytes + sizeof(common_len) + diff_len + total_used_bytes > B)
        {
			new_block(key, compressed_values, total_used_bytes); // create a new block
			n_strings += 1; // increment total strings
            return;
        }

		n_strings += 1; // increment total strings
        teletype.write((char*)&common_len, sizeof(common_len)); // write common length
        teletype.write(key.c_str() + common_len, diff_len); // write differing part of the key
            
        for(auto cValue : compressed_values)
            teletype.write((char*)cValue.bytes, cValue.used_bytes); // write compressed values

        current_bytes += sizeof(common_len) + diff_len + total_used_bytes; // update current bytes
    }
	// Finalizes the storage structure:
	// - Writes metadata and heads, updating the stream to prepare for disk storage.
    void finalize()
    {
        // Write the array of heads and save their offset
        align_stream_to_block(teletype);

		uint64_t offset_heads = teletype.tellp(); // calculate offset for heads
        for(auto& head_str : heads)
            teletype.write(head_str.c_str(), head_str.size() + 1); // write heads to stream

        // Write the metadata block
        teletype.seekp(metadata_block_off, std::ios_base::beg);

        teletype.write((char*)&n_strings, sizeof(uint64_t));
        teletype.write((char*)&offset_heads, sizeof(uint64_t));

        uint64_t n_blocks = heads.size();
        teletype.write((char*)&n_blocks, sizeof(uint64_t));

        teletype.flush(); // flush the stream
    }

};


/**
 * merge function combines multiple sorted ranges into a single disk map efficiently.
 * - Merges sorted ranges from various input iterators into a single disk map.
 * - Handles merging values with the same key based on user-defined merge policies.
 * @tparam Value
 * @tparam InputIterator
 * @tparam B
 * @tparam T
 * @param out_stream
 * @param maps
 * @param merge_policy
 * @param trasform_f
 */


template<class Value, class InputIterator, size_t B = BLOCK_SIZE, class T = Value>
void merge(
		std::ostream &out_stream,
		const std::vector<std::pair<InputIterator, InputIterator>>& maps,
		std::function<Value(const std::string&, const std::vector<Value>&)> merge_policy,
		std::function<Value(const T&)> trasform_f = nullptr)
{
	// Make sure input and output vals are the same
	if constexpr (std::is_same_v<Value, T>)
		static_assert(
			std::is_same_v<typename InputIterator::value_type, typename std::pair<std::string,Value>>,
			"Input and output values must have the same data type!");
	else
		static_assert(
			std::is_same_v<typename InputIterator::value_type, typename std::pair<std::string,T>>,
			"Check trasform_f input type");

	// Struct to store position information of iterators
	struct pos
	{
		InputIterator curr, end;
	};

	// Create a list of iterators to the input maps
	std::list<pos> positions;
	disk_map_writer<Value, B> global(out_stream);

	// Initialize positions with iterators from input maps
	for (auto& [begin, end] : maps)
		positions.push_back({begin, end});

	// Merge operation
	while(not positions.empty())
	{
		std::string min;

		// Find the minimum key
		for(const auto &p : positions)
			min = min.empty() ? p.curr->first : std::min(p.curr->first, min);

		// Collect all values with the same key
		std::vector<Value> values;
		for(auto it = positions.begin(); it != positions.end(); )
		{
			// Skip iterators that are not pointing to the current min
			if(it->curr->first != min)
			{
				++it;
				continue;
			}

			// Copy value directly if no trasform function is supplied
			if constexpr (std::is_same_v<Value, T>)
				values.push_back(it->curr->second);
			else // Otherwise trasform it
				values.push_back(trasform_f(it->curr->second));

			// Advance iterator and remove it if it's at the end
			++(it->curr);
			if(it->curr == it->end)
				it = positions.erase(it);
			else
				++it;
		}

		// Merge the values, if necessary; then add them to the output map
		Value merged = (values.size() == 1) ? values[0] : merge_policy(min, values);
		global.add(min, merged);
	}

	global.finalize(); // Finalize writing to disk
}

}
