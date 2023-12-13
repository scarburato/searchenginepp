#include "Index.hpp"

namespace sindex
{

template<>
std::vector<result_t> Index<SigmaLexiconValue>::query_bmm(std::set<std::string> &query, bool conj, size_t top_k)
{
	struct pending_result_t {
		docid_t docid;
		score_t score;

		bool operator>(const pending_result_t& b) const {return score > b.score;}
	};

	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be popped
	std::priority_queue<pending_result_t, std::vector<pending_result_t>, std::greater<>> results;

	struct PostingListHelper {PostingList pl; typename PostingList::iterator it;};
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
	size_t pivot = 0;
	const size_t N = posting_lists_its.size();
	score_t θ = 0.0;
	std::vector<score_t> upper_bounds;

	// Initialize the ubber bounds vector
	upper_bounds.push_back(posting_lists_its.begin()->pl.get_lexicon_value().bm25_sigma);
	for(auto it = posting_lists_its.begin(); it != posting_lists_its.end(); ++it)
	{
		if(it == posting_lists_its.begin())
			continue;

		upper_bounds.push_back(upper_bounds.back() + it->pl.get_lexicon_value().bm25_sigma);
	}

	// Iterate all documents 'til we exhaust them or the pruning condition is met
	while(pivot < N and not posting_lists_its.empty())
	{
		// ... @TODO @FIXME @URG @CRITICAL @THINKABOUTIT ...
	}

	return {};
}

}
