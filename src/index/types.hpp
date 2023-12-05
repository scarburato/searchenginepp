#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <array>
#include <vector>
#include "../codes/variable_blocks.hpp"

namespace sindex
{
typedef uint64_t docid_t;
typedef std::string docno_t;
typedef uint64_t doclen_t;
typedef size_t freq_t;
typedef double score_t;

constexpr docid_t DOCID_MAX = std::numeric_limits<docid_t>::max();

struct result_t
{
	docno_t docno;
	score_t score;

	bool operator>(const result_t &b) const {return score > b.score;}
	bool operator==(const result_t &b) const {return score == b.score;}
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

	static LexiconValue deserialize(const std::array<uint64_t, serialize_size>& ser)
	{
		return {ser[0], ser[1], ser[2], ser[3], ser[4]};
	}

};

struct SigmaLexiconValue : public LexiconValue
{
	score_t bm25_sigma = 0;
	score_t tfidf_sigma = 0;
	struct skip_pointer_t
	{
		score_t bm25_ub = 0;
		score_t tfidf_ub = 0;
		docid_t last_docid;
		size_t docid_offset;
		size_t freq_offset;
	};
	std::vector<skip_pointer_t> skip_pointers;

	static constexpr size_t serialize_size = 0;
	static constexpr size_t fixed_point_factor = 1e4;

	SigmaLexiconValue(const LexiconValue& lv) : LexiconValue(lv) 
	{};

	SigmaLexiconValue() = default;

	std::vector<uint64_t> serialize () const
	{
		std::vector<uint64_t> ser;

		// First part of data struct serialized as before
		ser.insert(ser.end(), LexiconValue::serialize().begin(), LexiconValue::serialize().end());
		
		// Global sigmas
		ser.push_back(static_cast<unsigned long>(bm25_sigma * fixed_point_factor));
		ser.push_back(static_cast<unsigned long>(tfidf_sigma * fixed_point_factor));

		// Add sigma values 'n skip list
		for (const auto& sp : skip_pointers)
			ser.insert(ser.end(), {
				static_cast<unsigned long>(sp.bm25_ub * fixed_point_factor),
				static_cast<unsigned long>(sp.tfidf_ub * fixed_point_factor),
				sp.last_docid,
				sp.docid_offset,
				sp.freq_offset
			});
		
		return ser;
	}

	static SigmaLexiconValue deserialize(const std::vector<uint64_t>& ser)
	{
		SigmaLexiconValue slv;
		slv.start_pos_docid = ser[0];
		slv.end_pos_docid = ser[1];
		slv.start_pos_freq = ser[2];
		slv.end_pos_freq = ser[3];
		slv.n_docs = ser[4];
		slv.bm25_sigma = ser[5] / static_cast<double>(fixed_point_factor);
		slv.tfidf_sigma = ser[6] / static_cast<double>(fixed_point_factor);

		// Deserialize skip list
		assert((ser.size() - 7) % 5 == 0);
		for (size_t i = 7; i < ser.size(); i += 5)
			slv.skip_pointers.push_back({
				.bm25_ub = ser[i] / static_cast<double>(fixed_point_factor),
				.tfidf_ub = ser[i + 1] / static_cast<double>(fixed_point_factor),
				.last_docid = static_cast<docid_t>(ser[i + 2]),
				.docid_offset = ser[i + 3],
				.freq_offset = ser[i + 4]
			});
		
		return slv;
	}
};

template<typename EncondedDataIterator>
using IndexDecoder = codes::VariableBlocksDecoder<EncondedDataIterator, docid_t>;

template<typename RawDataIterator>
using IndexEncoder = codes::VariableBlocksEncoder<RawDataIterator>;

}
