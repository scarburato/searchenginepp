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

Index::Index(Index::local_lexicon_t lx, Index::global_lexicon_t &gx, const memory_area &iid,
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

Index::~Index()
{

}

std::vector<result_t> Index::query(std::set<std::string> &query, bool conj, size_t top_k)
{
	struct pending_result_t {
		docid_t docid;
		score_t score;

		bool operator>(const pending_result_t& b) const {return score > b.score;}
	};

	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be popped
	std::priority_queue<pending_result_t, std::vector<pending_result_t>, std::greater<>> results;

	struct PostingListHelper {PostingList pl; PostingList::iterator it;};
	std::list<PostingListHelper> posting_lists_its; //@fixme make it work with std::vector
	// size_t n_docs_to_process = 0; // Never used
	docid_t docid_base = -1;

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
		auto it = pl.begin();

		// n_docs_to_process = std::max(n_docs_to_process, posting_info.n_docs);

		docid_base = std::min(docid_base, it->first);

		posting_lists_its.push_back({pl, it});

		++q_term_it;
	}

	docid_t curr_docid = docid_base;

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
			if(results.empty() or score > results.top().score)
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
			posting_helper_it->it.nextG(curr_docid, posting_helper_it->pl.end());

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

Index::PostingList::PostingList(Index const *index, const std::string& term, const LexiconValue& lv):
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

Index::PostingList::iterator Index::PostingList::begin() const
{
	auto it = iterator(docid_dec.begin(), freq_dec.begin());
	it.current = std::make_pair(*it.docid_curr, *it.freq_curr);
	return it;
}

Index::PostingList::iterator Index::PostingList::end() const
{
	return {docid_dec.end(), freq_dec.end()};
}

score_t Index::PostingList::score(const Index::PostingList::iterator& it, const QueryScorer& scorer) const
{
	doclen_t dl = scorer.needs_doc_metadata() ? index->document_index[it.current.first - index->base_docid].lenght : 0;
	return scorer.score(it.current.second, idf, dl, index->avgdl);
}

Index::PostingList::offset Index::PostingList::get_offset(const Index::PostingList::iterator& it)
{
	return {
		.docid_off = static_cast<uint64_t>(it.docid_curr.get_raw_iterator() - docid_dec.begin().get_raw_iterator()),
		.freq_off = codes::serialize_bit_offset(
				it.freq_curr.get_raw_iterator() - freq_dec.begin().get_raw_iterator(),
				it.freq_curr.get_bit_offset())
	};
}

void Index::PostingList::iterator::nextG(docid_t docid, const iterator& end)
{
	while(*this != end and current.first <= docid)
		++*this;
}

} // namespace sindex
