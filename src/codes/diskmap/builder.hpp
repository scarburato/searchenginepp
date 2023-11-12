#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <sys/types.h>
#include <vector>
#include "../variable_blocks.hpp"

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
        auto cValues = codes::VariableBytes(value);
        n_strings += 1;

        if(heads.empty())
        {
            heads.push_back(key);

            teletype.put(0);
            teletype.write((char*)cValues.bytes, sizeof(cValues.bytes));

            current_bytes = 1 + sizeof(cValues);
            i += 1;
            return;
        }

        size_t key_len = key.size() + 1;
        uint8_t common_len = compute_common_prefix(key.c_str(), heads.back().c_str());
        size_t diff_len = key_len - common_len;

        if(current_bytes + sizeof(common_len) + diff_len + sizeof(cValues)> BLOCK_SIZE)
        {
            heads.push_back(key);
            align_stream_to_block(teletype);

            auto ci = codes::VariableBytes(i++);

            teletype.write((char*)ci.bytes, ci.used_bytes);
            teletype.write((char*)cValues.bytes, cValues.used_bytes);

            current_bytes = ci.used_bytes + sizeof(cValues);
            return;
        }

        teletype.write((char*)&common_len, sizeof(common_len));
        teletype.write((char*)cValues.bytes, cValues.used_bytes);
        teletype.write(key.c_str() + common_len, diff_len);

        current_bytes += sizeof(common_len) + diff_len + sizeof(cValues);
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

