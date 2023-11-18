#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <array>
#include "../codes/variable_blocks.hpp"

namespace sindex
{
typedef uint32_t docid_t;
typedef std::string docno_t;
typedef uint64_t doclen_t;
typedef size_t freq_t;
typedef double score_t;

struct result_t
{
	docno_t docno;
	score_t score;
};

struct DocumentInfoSerialized
{
	size_t docno_offset;
	doclen_t lenght;
};

struct LexiconValue
{
	size_t start_pos_docid;
	size_t end_pos_docid;
	size_t start_pos_freq;
	size_t end_pos_freq;
	freq_t n_docs;

	static constexpr size_t serialize_size = 5;

	std::array<uint64_t, serialize_size> serialize () const
	{
		return {start_pos_docid, end_pos_docid, start_pos_freq, end_pos_freq, n_docs};
	}
};

template<typename EncondedDataIterator>
using IndexDecoder = codes::VariableBlocksDecoder<EncondedDataIterator, docid_t>;

template<typename RawDataIterator>
using IndexEncoder = codes::VariableBlocksEncoder<RawDataIterator>;

}
