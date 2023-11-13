#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <ostream>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>
#include "../variable_blocks.hpp"
#include "../../meta.hpp"

constexpr size_t BLOCK_SIZE = 1024;

template<class Value, size_t B = BLOCK_SIZE>
class disk_map_writer
{
private:
    std::ostream& teletype;
    off_t metadata_block_off;
    std::vector<std::string> heads;
    size_t current_bytes = 0;
    size_t i = 0;
    size_t n_strings = 0;

    static constexpr size_t N = [] {
		if constexpr (std::is_integral_v<Value>)
			return 1;

		if constexpr (is_std_array_v<Value>)
			return Value().size();


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
        size_t next_block_off = BLOCK_SIZE - ostr.tellp() % BLOCK_SIZE;
        if(next_block_off != BLOCK_SIZE)
            ostr.seekp(next_block_off, std::ios_base::cur);	
    }

public:
    disk_map_writer(std::ostream& teletype):
        teletype(teletype)
    {
        metadata_block_off = teletype.tellp();
        teletype.seekp(B, std::ios_base::cur);
    }

    void add(const std::string& key, Value value)
    {
        // auto cValues = codes::VariableBytes(value);

        std::array<codes::VariableBytes, N> cValues;
        if constexpr (std::is_integral_v<Value> or std::is_array_v<Value>)
            for(auto val : value)
                cValues.push_back(codes::VariableBytes(val));
        else // Object
            for(auto val : value.serialize())
                cValues.push_back(codes::VariableBytes(val));

        // Sum of total used bytes of all the values
        size_t total_used_bytes = std::accumulate(cValues.begin(), cValues.end(), 0, [](size_t sum, const auto& cValue) { return sum + cValue.used_bytes; });
        
        n_strings += 1;

        if(heads.empty())
        {
            heads.push_back(key);

            teletype.put(0);
            for(auto cValue : cValues)
                teletype.write((char*)cValue.bytes, cValue.used_bytes);

            current_bytes = 1 + total_used_bytes;
            
            i += 1;
            return;
        }

        size_t key_len = key.size() + 1;
        uint8_t common_len = compute_common_prefix(key.c_str(), heads.back().c_str());
        size_t diff_len = key_len - common_len;

        if(current_bytes + sizeof(common_len) + diff_len + total_used_bytes> BLOCK_SIZE)
        {
            heads.push_back(key);
            align_stream_to_block(teletype);

            auto ci = codes::VariableBytes(i++);

            teletype.write((char*)ci.bytes, ci.used_bytes);
            for(auto cValue : cValues)
                teletype.write((char*)cValue.bytes, cValue.used_bytes);

            current_bytes = ci.used_bytes + total_used_bytes;
            return;
        }

        teletype.write((char*)&common_len, sizeof(common_len));
        teletype.write(key.c_str() + common_len, diff_len);
        for(auto cValue : cValues)
            teletype.write((char*)cValue.bytes, cValue.used_bytes);

        current_bytes += sizeof(common_len) + diff_len + total_used_bytes;
    }

    void finalize()
    {
        // Write the array of heads and save their offset
        align_stream_to_block(teletype);

        auto offset_heads = teletype.tellp();
        for(auto& head_str : heads)
            teletype.write(head_str.c_str(), head_str.size() + 1);

        // Write the metadata block
        teletype.seekp(metadata_block_off, std::ios_base::beg);

        teletype.write((char*)&n_strings, sizeof(n_strings));
        teletype.write((char*)&offset_heads, sizeof(offset_heads));

        uint64_t n_blocks = heads.size(); 
        teletype.write((char*)&n_blocks, sizeof(n_blocks));

    } 
};

