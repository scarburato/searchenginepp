#pragma once

#include <string>
#include <limits>
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

// Maximum value of docid_t, representing the maximum possible document identifier
constexpr docid_t DOCID_MAX = std::numeric_limits<docid_t>::max();
/*
    This struct represents a result entry consisting of two fields:
    - 'docno' of type 'docno_t' (which is typically a string representing a document number or identifier).
    - 'score' of type 'score_t' (a numerical value representing the score associated with the document).

    It defines comparison operators (greater-than and equality) for result_t instances based on the 'score' field.
    These operators are useful for comparing and ordering result_t objects based on their scores.
*/
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
/*
	LexiconValue struct represents metadata about a lexicon entry:
	- 'start_pos_docid', 'end_pos_docid', 'start_pos_freq', 'end_pos_freq': Size information or positions
			related to document IDs and their frequencies within the index structure.
	- 'n_docs': Number of documents associated with the lexicon entry.

	It includes methods 'serialize' and 'deserialize' to convert the struct into an array of uint64_t
			and vice versa, useful for serialization/deserialization purposes.
	*/
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
	using skip_list_t = std::vector<skip_pointer_t>;
	skip_list_t skip_pointers;

	static constexpr size_t serialize_size = 0;
	static constexpr size_t fixed_point_factor = 1e2;

	SigmaLexiconValue(const LexiconValue& lv) : LexiconValue(lv) 
	{};

	SigmaLexiconValue() = default;

	/*
		This method serializes the struct's data, including global sigmas and skip pointers, into a vector
	    of uint64_t values. The data is prepared for storage or transmission in a serialized form.

         - The first part of the struct is serialized as before using LexiconValue::serialize().
         - Global sigmas (bm25_sigma and tfidf_sigma) are converted to fixed-point integers and added to 'ser'.
         - For each skip pointer in 'skip_pointers', its respective data is serialized:
         - 'bm25_ub' and 'tfidf_ub' are converted to fixed-point integers and added to 'ser'.
	     - 'last_docid', 'docid_offset', and 'freq_offset' are added to 'ser'.
	 */
	std::vector<uint64_t> serialize () const
	{
		std::vector<uint64_t> ser;
		ser.reserve(LexiconValue::serialize_size + skip_pointers.size() * 5);

		// First part of data struct serialized as before
		auto ser_base = LexiconValue::serialize();
		ser.insert(ser.end(), ser_base.begin(), ser_base.end());
		
		// Global sigmas
		ser.push_back(static_cast<uint64_t>(bm25_sigma * fixed_point_factor));
		ser.push_back(static_cast<uint64_t>(tfidf_sigma * fixed_point_factor));

		// Add sigma values 'n skip list
		for (const auto& sp : skip_pointers)
			ser.insert(ser.end(), {
				static_cast<uint64_t>(sp.bm25_ub * fixed_point_factor),
				static_cast<uint64_t>(sp.tfidf_ub * fixed_point_factor),
				sp.last_docid,
				sp.docid_offset,
				sp.freq_offset
			});
		
		return ser;
	}
/*
    This static method constructs a SigmaLexiconValue object by deserializing a vector of uint64_t values.
    - The method assumes that the provided vector 'ser' contains serialized data, where:
        - Indices 0 to 4 store information for LexiconValue deserialization.
        - Index 5 holds serialized data representing 'bm25_sigma'.
        - Index 6 holds serialized data representing 'tfidf_sigma'.
        - Following the global sigmas, the rest of 'ser' stores skip pointers data in multiples of 5 values per skip pointer.
    - The method initializes 'slv' by deserializing the initial LexiconValue part from 'ser'.
    - 'bm25_sigma' and 'tfidf_sigma' are reconstructed from fixed-point integers into their original double representations.
    - The method validates the skip pointers' serialized data's size to ensure correct deserialization.
    - Each skip pointer data (in groups of 5 values) is deserialized and added to 'slv.skip_pointers', reconstructing the skip list:
        - 'bm25_ub' and 'tfidf_ub' are restored to their double representations from fixed-point integers.
        - 'last_docid' is cast to 'docid_t', while 'docid_offset' and 'freq_offset' remain as uint64_t.
    - Finally, the fully reconstructed SigmaLexiconValue 'slv' is returned, containing the deserialized data.
*/
	static SigmaLexiconValue deserialize(const std::vector<uint64_t>& ser)
	{
		SigmaLexiconValue slv = LexiconValue::deserialize({ser[0], ser[1], ser[2], ser[3], ser[4]});
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

}
