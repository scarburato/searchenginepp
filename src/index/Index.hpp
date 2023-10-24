#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../codes/variable_blocks.hpp"
#include "types.hpp"

namespace sindex
{

struct DocumentInfo
{
	docno_t docno;
	doclen_t lenght;
};

/**
 * This class represents the index and is used to access the documents'
 * information regarding the terms they contain and their frequencies.
 */
class Index{
private:
	std::unordered_map<std::string, std::pair<size_t, size_t>> term_to_inverted_index;

	uint8_t *inverted_indices;
	size_t inverted_indices_length;

	uint8_t *inverted_indices_freqs;
	size_t inverted_indices_freqs_length;

	std::vector<DocumentInfo> document_index;

public:
	/**
	 * Create an Index from files
	 * @param map_filename
	 * @param inverted_indices_filename
	 */
	Index(const std::string& map_filename, const std::string& inverted_indices_filename);
	~Index();
};

}