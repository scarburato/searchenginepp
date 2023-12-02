#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include "../codes/diskmap/diskmap.hpp"
#include "../codes/variable_blocks.hpp"
#include "../codes/unary.hpp"
#include "types.hpp"
#include "../util/memory.hpp"
#include "query_scorer.hpp"

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
public:
	using local_lexicon_t = codes::disk_map<LexiconValue>;
	using global_lexicon_t = codes::disk_map<freq_t>;

private:
	docid_t base_docid;
	size_t n_docs;
	double avgdl;

	local_lexicon_t local_lexicon;
	global_lexicon_t& global_lexicon;

	const uint8_t *inverted_indices;
	size_t inverted_indices_length;

	const uint8_t *inverted_indices_freqs;
	size_t inverted_indices_freqs_length;

	// Document index
	const DocumentInfoSerialized *document_index;
	size_t document_index_length;

	// A series of docno strs, the offsets are written in document_index's elements
	const char* base_docno;

	QueryScorer& scorer;
public:
	/**
	 * @param lx the local lexicon
	 * @param gx the global lexicon
	 * @param iid inverted indices docids
	 * @param iif inverted indices freqs
	 * @param di document index
	 * @param metadata metadata (N, sigma, avgdl, etc...)
	 * @param qs query_scorer to use
	 */
	Index(local_lexicon_t lx, global_lexicon_t& gx, const memory_area& iid, const memory_area& iif,
		  const memory_area& di, const memory_area& metadata, QueryScorer& qs);
	~Index();

	void set_scorer(QueryScorer& qs) {scorer = qs;}
	std::vector<result_t> query(std::set<std::string>& query, size_t top_k = 10);

	class PostingList
	{
		using docid_decoder_t = codes::VariableBlocksDecoder<const uint8_t*>;
		using freq_decoder_t = codes::UnaryDecoder<const uint8_t*>;
		Index *index;
		docid_decoder_t::DecodeIterator docid_begin, docid_end;
		freq_decoder_t::DecodeIterator freq_begin, freq_end;


	public:
		class iterator
		{
			docid_decoder_t::DecodeIterator docid_curr;
			freq_decoder_t::DecodeIterator freq_curr;
		};

		PostingList(Index* index, docid_decoder_t docid_dec, freq_decoder_t freq_dec):
			index(index), docid_begin(docid_dec.begin()), docid_end(docid_dec.end()),
			freq_begin(freq_dec.begin()), freq_end(freq_dec.end()) {}

		iterator begin() const;
		iterator end() const;

		static PostingList create(Index* index, LexiconValue& lv);
	};
};

}
