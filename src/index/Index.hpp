#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>
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
template<class LVT = LexiconValue>
class Index{
public:
	using local_lexicon_t = codes::disk_map<LVT>;
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
	std::vector<result_t> query(std::set<std::string>& query, bool and_mode = false, size_t top_k = 10);

	class PostingList
	{
		using docid_decoder_t = codes::VariableBlocksDecoder<const uint8_t*>;
		using freq_decoder_t = codes::UnaryDecoder<const uint8_t*>;
		
		Index const *index;
		LexiconValue lv;
		double idf;

		docid_decoder_t docid_dec;
		freq_decoder_t freq_dec;

	public:
		struct offset {uint64_t docid_off; uint64_t freq_off;};

		class iterator
		{
			docid_decoder_t::iterator docid_curr;
			freq_decoder_t::iterator freq_curr;

			std::pair<docid_t, freq_t> current;

			iterator(docid_decoder_t::iterator docid_curr, freq_decoder_t::iterator freq_curr):
					docid_curr(docid_curr), freq_curr(freq_curr)
			{}
		public:

		 	const std::pair<docid_t, freq_t>& operator*() const {return current;}
			const std::pair<docid_t, freq_t>* operator->() const {return &current;}

			iterator& operator++() 
			{
				++docid_curr;
				++freq_curr;
				current.first = *docid_curr;
				current.second = *freq_curr;
				return *this;
			}

			iterator operator+(unsigned n) const
			{
				iterator tmp(*this);
				for(unsigned i = 0; i < n; ++i)
					++tmp;
				return tmp;
			}

			bool operator==(const iterator& b) const {return docid_curr == b.docid_curr;}
			bool operator!=(const iterator& b) const {return !(*this == b);}

			void nextG(docid_t, const iterator&);

			friend PostingList;
		};


		PostingList(Index const *index, const std::string& term, const LexiconValue& lv);
		score_t score(const PostingList::iterator& it, const QueryScorer& scorer) const;

		iterator begin() const;
		iterator end() const;

		/** This stuff assumes that the freq decoder's begin is aligned (ie its offset is 0) */
		offset get_offset(const iterator&);
	};

	PostingList get_posting_list(const std::string& term, const LexiconValue& lv) const {return PostingList(this, term, lv);}
	local_lexicon_t& get_local_lexicon() {return local_lexicon;}
};

}

#include "Index.template.hpp"
