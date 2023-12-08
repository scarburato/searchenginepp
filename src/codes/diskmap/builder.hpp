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

template<class Value, size_t B = BLOCK_SIZE>
class disk_map_writer
{
private:
    std::ostream& teletype;
    off_t metadata_block_off;
    std::vector<std::string> heads;
    size_t current_bytes = 0;
	uint64_t n_strings = 0;
	[[maybe_unused]] std::string debug_last_string;

    static constexpr size_t N = [] {
		if constexpr (std::is_integral_v<Value>)
			return 1;
		else if constexpr (is_std_array_v<Value>)
			return Value().size();
		else
			return Value::serialize_size;
	}();

    static size_t compute_common_prefix(const char *a, const char *b)
    {
        size_t common_len = 0;
        for(; a[common_len] and b[common_len] and a[common_len] == b[common_len]; ++common_len)
            continue;

        return common_len;
    }

    static void align_stream_to_block(std::ostream& ostr)
    {
        size_t next_block_off = B - ostr.tellp() % B;
        if(next_block_off != B)
            ostr.seekp(next_block_off, std::ios_base::cur);
    }

    template<class Container>
    void new_block(const std::string& key, Container& compressed_values, size_t cvals_size)
    {
        heads.push_back(key);
        align_stream_to_block(teletype);

        auto bi_encoded = codes::VariableBytes(n_strings);

        teletype.write((char*)bi_encoded.bytes, bi_encoded.used_bytes);

        for(auto cValue : compressed_values)
            teletype.write((char*)cValue.bytes, cValue.used_bytes);
        
        current_bytes = bi_encoded.used_bytes + cvals_size;
    }

public:
	disk_map_writer() = delete;
    explicit disk_map_writer(std::ostream& teletype):
        teletype(teletype)
    {
        metadata_block_off = teletype.tellp();
        teletype.seekp(B, std::ios_base::cur);
    }

    void add(const std::pair<std::string, Value>& p) {add(p.first, p.second);}

    void add(const std::string& key, const Value& value)
    {
		assert(not key.empty() and key > debug_last_string and key.size() < 255);
		debug_last_string = key;

		// Create compressed_values as an array or vector based on the value of N
        std::conditional_t<(N == 0), std::vector<codes::VariableBytes>, std::array<codes::VariableBytes, N>> compressed_values;
        if constexpr (N != 0)
        {
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

		// First call to add()
        if(heads.empty())
        {
			new_block(key, compressed_values, total_used_bytes);
			n_strings += 1;
            return;
        }

        size_t key_len = key.size() + 1;
        uint8_t common_len = compute_common_prefix(key.c_str(), heads.back().c_str());
        size_t diff_len = key_len - common_len;

        if(current_bytes + sizeof(common_len) + diff_len + total_used_bytes > B)
        {
			new_block(key, compressed_values, total_used_bytes);
			n_strings += 1;
            return;
        }

		n_strings += 1;
        teletype.write((char*)&common_len, sizeof(common_len));
        teletype.write(key.c_str() + common_len, diff_len);
            
        for(auto cValue : compressed_values)
            teletype.write((char*)cValue.bytes, cValue.used_bytes);

        current_bytes += sizeof(common_len) + diff_len + total_used_bytes;
    }

    void finalize()
    {
        // Write the array of heads and save their offset
        align_stream_to_block(teletype);

		uint64_t offset_heads = teletype.tellp();
        for(auto& head_str : heads)
            teletype.write(head_str.c_str(), head_str.size() + 1);

        // Write the metadata block
        teletype.seekp(metadata_block_off, std::ios_base::beg);

        teletype.write((char*)&n_strings, sizeof(uint64_t));
        teletype.write((char*)&offset_heads, sizeof(uint64_t));

        uint64_t n_blocks = heads.size();
        teletype.write((char*)&n_blocks, sizeof(uint64_t));

        teletype.flush();
    }

};


/**
 * Merges multiple sorted ranges into a single one disk_map
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

	struct pos
	{
		InputIterator curr, end;
	};

	// Create a list of iterators to the input maps
	std::list<pos> positions;
	disk_map_writer<Value, B> global(out_stream);

	for (auto& [begin, end] : maps)
		positions.push_back({begin, end});

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

	global.finalize();
}

}
