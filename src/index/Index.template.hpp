#pragma once

#include <cstddef>
#include <set>
#include <list>
#include <queue>
#include <utility>
#include "Index.hpp"
#include "../util/memory.hpp"
#include "types.hpp"
#include "../codes/unary.hpp"

namespace sindex
{

template<class LVT>
Index<LVT>::Index(local_lexicon_t lx, global_lexicon_t &gx, const memory_area &iid,
			 const memory_area &iif, const memory_area &di, const memory_area& metadata, QueryScorer& qs):
	local_lexicon(std::move(lx)), global_lexicon(gx), scorer(qs)
{
	auto t = iid.get();
	inverted_indices = t.first;
	inverted_indices_length = t.second;

	t = iif.get();
	inverted_indices_freqs = t.first;
	inverted_indices_freqs_length = t.second;

	t = di.get();
	base_docid = *(docid_t*)t.first;
	document_index_length = *(size_t*)(t.first + sizeof(docid_t));
	document_index = (DocumentInfoSerialized*)(t.first + sizeof(docid_t) + sizeof(size_t));
	base_docno = (const char*)t.first + sizeof(docid_t) + sizeof(size_t) + document_index_length * sizeof(DocumentInfoSerialized);

	t = metadata.get();
	n_docs = *(size_t*)(t.first + sizeof(doclen_t));
	avgdl = (double)(*(doclen_t*)t.first) / n_docs;
}

template<class LVT>
Index<LVT>::~Index()
{

}

template<class LVT>
std::pair<std::list<typename Index<LVT>::PostingListHelper>, docid_t> Index<LVT>::build_helpers(std::set<std::string> &query, bool conj)
{
	std::list<PostingListHelper> posting_lists_its;
	// size_t n_docs_to_process = 0; // Never used
	docid_t docid_base = DOCID_MAX;

	// Iterate over all query terms. We remove useless terms and create the iterators of their posting lists
	for(auto q_term_it = query.begin(); q_term_it != query.end();)
	{
		auto posting_info_it = local_lexicon.find(*q_term_it);

		// Element not in lexicon, we'll not consider it
		if(posting_info_it == local_lexicon.end())
		{
			// If conjunctive mode, we can stop here
			if(conj)
				return {};

			q_term_it = query.erase(q_term_it);
			continue;
		}

		// Create 'n load posting list's info into vector
		const auto& [term, posting_info] = *posting_info_it;

		PostingList pl(this, term, posting_info);
		// n_docs_to_process = std::max(n_docs_to_process, posting_info.n_docs);
		posting_lists_its.emplace_back(std::move(pl));
		const auto& it = posting_lists_its.back().it;

		docid_base = std::min(docid_base, it->first);

		++q_term_it;
	}

	return {std::move(posting_lists_its), docid_base};
}

template<class LVT>
std::vector<result_t> Index<LVT>::query(std::set<std::string> query, bool conj, size_t top_k)
{
	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be popped
	pending_results_t results;

	auto [posting_lists_its, min_docid] = build_helpers(query, conj);
	docid_t curr_docid = min_docid;

	if(posting_lists_its.empty())
		return {};

	// Iterate all documents 'til we exhaust them
	// O(|D| * |Q| * log(|K|))
	while(not posting_lists_its.empty())
	{
		score_t score = 0;

		bool contains_all_terms = std::all_of(posting_lists_its.begin(), posting_lists_its.end(),
				[&curr_docid](const PostingListHelper& pl) {return pl.it->first == curr_docid;});

		// Score current document
		if(not conj or contains_all_terms)
		{
			for(auto& posting_helper : posting_lists_its)
			{
				const auto& [docid, freq] = *posting_helper.it;
				if(docid != curr_docid)
					continue;

				score += posting_helper.pl.score(posting_helper.it, scorer);
			}

			// Push computed result in the results, only if our score is greater than worst scoring doc in results
			if(results.size() < top_k or score > results.top().score)
			{
				results.emplace(curr_docid, score);

				// If necessary pop-out the worst scoring element
				if (results.size() > top_k)
					results.pop();
			}
		}

		docid_t next_docid = DOCID_MAX;
		// Move iterators to current docid or (the next closest one)
		for(auto posting_helper_it = posting_lists_its.begin(); posting_helper_it != posting_lists_its.end(); )
		{
			posting_helper_it->it.nextG(curr_docid);

			// We exhausted this posting list, let's remove it
			if(posting_helper_it->it == posting_helper_it->pl.end())
			{
				posting_helper_it = posting_lists_its.erase(posting_helper_it);
				continue;
			}

			next_docid = std::min(next_docid, posting_helper_it->it->first);

			// Continue
			++posting_helper_it;
		}
		curr_docid = next_docid;
	}

	return convert_results(results, top_k);
}

template<class LVT>
Index<LVT>::PostingList::PostingList(Index const *index, const std::string& term, const LVT& lv):
	index(index), lv(lv),
	docid_dec(index->inverted_indices + lv.start_pos_docid, index->inverted_indices + lv.end_pos_docid),
	freq_dec(index->inverted_indices_freqs + lv.start_pos_freq, index->inverted_indices_freqs + lv.end_pos_freq)
{
	// Retrive n_i from global lexicon
	auto global_term_info_it = index->global_lexicon.find(term);
	if (global_term_info_it == index->global_lexicon.end()) // IMPOSSIBLE!
			abort();

	// From n_i compute compute this posting list's IDF
	idf = QueryTFIDFScorer::idf(index->n_docs, global_term_info_it->second);
}

template<class LVT>
typename Index<LVT>::PostingList::iterator Index<LVT>::PostingList::begin() const
{
	auto it = iterator{this, docid_dec.begin(), freq_dec.begin()};
	it.current = {*it.docid_curr, *it.freq_curr};
	return it;
}

template<class LVT>
typename Index<LVT>::PostingList::iterator Index<LVT>::PostingList::end() const
{
	return {this, docid_dec.end(), freq_dec.end()};
}

template<class LVT>
score_t Index<LVT>::PostingList::score(const Index::PostingList::iterator& it, const QueryScorer& scorer) const
{
	doclen_t dl = scorer.needs_doc_metadata() ? index->document_index[it.current.first - index->base_docid].lenght : 0;
	return scorer.score(it.current.second, idf, dl, index->avgdl);
}

template<class LVT>
typename Index<LVT>::PostingList::offset Index<LVT>::PostingList::get_offset(const Index::PostingList::iterator& it)
{
	return {
		.docid_off = static_cast<uint64_t>(it.docid_curr.get_raw_iterator() - docid_dec.begin().get_raw_iterator()),
		.freq_off = codes::serialize_bit_offset(
				it.freq_curr.get_raw_iterator() - freq_dec.begin().get_raw_iterator(),
				it.freq_curr.get_bit_offset())
	};
}

template<class LVT>
void Index<LVT>::PostingList::iterator::nextG(docid_t docid)
{
	while(*this != parent->end() and current.first <= docid)
		++*this;
}

template<class LVT>
void Index<LVT>::PostingList::iterator::nextGEQ(docid_t docid)
{
	while(*this != parent->end() and current.first < docid)
		++*this;
}

} // namespace sindex
