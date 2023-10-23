#pragma once

#include <cstdint>
#include <map>
#include "../codes/variable_blocks.hpp"

namespace index
{
typedef uint32_t docid_t;
typedef uint32_t docno_t;

template<typename EncondedDataIterator>
using IndexDecoder = codes::VariableBlocksDecoder<EncondedDataIterator, docid_t>;

template<typename RawDataIterator>
using IndexEncoder = codes::VariableBlocksEncoder<RawDataIterator>;

class Index{
private:
    std::map<std::string, std::pair<std::streampos, std::streampos>> term_to_inverted_index;
    std::unique_ptr<std::istream> inverted_indices;
public:
    Index(const std::string& map_filename, const std::string& inverted_indices_filename){
        // Open the file streams
        std::ifstream map_file(map_filename, std::ios::binary);
        std::ifstream inverted_indices_file(inverted_indices_filename, std::ios::binary);

        if (!map_file.is_open() || !inverted_indices_file.is_open()) {
            throw std::runtime_error("Failed to open file");
        }

        // Read data from the map file using IndexDecoder
        IndexDecoder<std::istreambuf_iterator<char>> index_decoder(map_file, std::istreambuf_iterator<char>());
        // Perform operations to fill term_to_inverted_index using index_decoder

        // Save the inverted indices file stream
        inverted_indices = std::make_unique<std::ifstream>(std::move(inverted_indices_file));

        // Close the file streams
        map_file.close();
    }
};
}