#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>
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

	struct pending_result_t {
		docid_t docid;
		score_t score;

		bool operator>(const pending_result_t& b) const {return score > b.score;}
	};

	struct PostingListHelper;

	/**
	 * Build the posting lists' iterators for the given query. If in conj mode it returns an empty list if one of the
	 * terms is not in the lexicon.
	 */
	std::pair<std::list<PostingListHelper>, docid_t> build_helpers(std::set<std::string> &query, bool conj = false);

	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be popped
	using pending_results_t = std::priority_queue<pending_result_t, std::vector<pending_result_t>, std::greater<>>;

	std::vector<result_t> convert_results(pending_results_t& results, size_t top_k)
	{
		// Build results
		std::vector<result_t> final_results;
		final_results.reserve(top_k);

		while(not results.empty())
		{
			auto res = results.top();
			results.pop();

			result_t res_f = {
					.docno = std::string(base_docno + document_index[res.docid - base_docid].docno_offset),
					.score = res.score
			};

			// Results are read in increasing order, we have to push them in front, to have descending order
			final_results.insert(final_results.begin(), res_f);
		}

		return final_results;
	}
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
	std::vector<result_t> query(std::set<std::string>& query, bool conj = false, size_t top_k = 10);
	std::vector<result_t> query_bmm(std::set<std::string>& query, size_t top_k = 10);

	class PostingList
	{
		using docid_decoder_t = codes::VariableBlocksDecoder<const uint8_t*>;
		using freq_decoder_t = codes::UnaryDecoder<const uint8_t*>;
		
		Index const *index;
		LVT lv;
		double idf;

		docid_decoder_t docid_dec;
		freq_decoder_t freq_dec;

	public:
		struct offset {uint64_t docid_off; uint64_t freq_off;};
		struct value {docid_t docid; freq_t freq;};

		class iterator
		{
			PostingList const *parent;
			// Current block ptr to iterator. Only used in skip list specialization
			SigmaLexiconValue::skip_list_t::const_iterator current_block_it;
			docid_decoder_t::iterator docid_curr;
			freq_decoder_t::iterator freq_curr;

			std::pair<docid_t, freq_t> current;

			iterator(PostingList const *parent, docid_decoder_t::iterator docid_curr, freq_decoder_t::iterator freq_curr):
					parent(parent), docid_curr(docid_curr), freq_curr(freq_curr)
			{}

			iterator(PostingList const *parent, SigmaLexiconValue::skip_list_t::const_iterator current_block_it, docid_decoder_t::iterator docid_curr, freq_decoder_t::iterator freq_curr):
					parent(parent), current_block_it(current_block_it), docid_curr(docid_curr), freq_curr(freq_curr)
			{}

			void skip_block() {abort();};
		public:

		 	const std::pair<docid_t, freq_t>& operator*() const {return current;}
			const std::pair<docid_t, freq_t>* operator->() const {return &current;}

			iterator& operator++() 
			{
				++docid_curr;
				++freq_curr;

				// Parse
				if(*this != parent->end())
				{
					current.first = *docid_curr;
					current.second = *freq_curr;
				}
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

			void nextG(docid_t);
			void nextGEQ(docid_t);

			friend PostingList;
		};


		PostingList(Index const *index, const std::string& term, const LVT& lv);
		score_t score(const PostingList::iterator& it, const QueryScorer& scorer) const;

		iterator begin() const;
		iterator end() const;

		/** This stuff assumes that the freq decoder's begin is aligned (ie its offset is 0) */
		offset get_offset(const iterator&);
		const LVT& get_lexicon_value() const {return lv;}
		const SigmaLexiconValue::skip_pointer_t& get_block([[maybe_unused]] const iterator& it) const {abort();}
	};

	PostingList get_posting_list(const std::string& term, const LexiconValue& lv) const {return PostingList(this, term, lv);}
	local_lexicon_t& get_local_lexicon() {return local_lexicon;}

private:
	struct PostingListHelper
	{
		PostingList pl; typename PostingList::iterator it;

		explicit PostingListHelper(PostingList&& pl): pl(std::move(pl)), it(this->pl.begin()) {}
	};
};

/** Specializations for skipping lists **/

template<>
Index<SigmaLexiconValue>::PostingList::iterator Index<SigmaLexiconValue>::PostingList::begin() const;

template<>
Index<SigmaLexiconValue>::PostingList::iterator Index<SigmaLexiconValue>::PostingList::end() const;

template<>
Index<SigmaLexiconValue>::PostingList::iterator& Index<SigmaLexiconValue>::PostingList::iterator::operator++();

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::nextG(sindex::docid_t);

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::nextGEQ(sindex::docid_t);

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::skip_block();

}

#include "Index.template.hpp"
