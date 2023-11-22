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

std::vector<result_t> Index::query(std::set<std::string> &query, size_t top_k)
{
	using docid_decoder_t = codes::VariableBlocksDecoder<const uint8_t*>;
	using freq_decoder_t = codes::UnaryDecoder<const uint8_t*>;
	using decoder_its_t = struct {
		docid_decoder_t::DecodeIterator docid_curr, docid_end;
		freq_decoder_t::DecodeIterator freq_cur, freq_end;
		freq_t document_freq;
		double idf;
	};

	struct pending_result_t {
		docid_t docid;
		score_t score;

		bool operator>(const pending_result_t& b) const {return score > b.score;}
	};

	const bool doclen_required = scorer.needs_doc_metadata();

	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be pop
	std::priority_queue<pending_result_t, std::vector<pending_result_t>, std::greater<>> results;

	std::list<decoder_its_t> posting_lists_its; //@fixme make it work with std::vector
	size_t n_docs_to_process = 0;
	docid_t docid_base = -1;

	// Iterate over all query terms. We remove useless terms and create the iterators of their posting lists
	for(auto q_term_it = query.begin(); q_term_it != query.end();)
	{
		auto posting_info_it = local_lexicon.find(*q_term_it);

		// Element not in lexicon, we'll not consider it
		if(posting_info_it == local_lexicon.end())
		{
			q_term_it = query.erase(q_term_it);
			continue;
		}

		// Look this term's doc. freq
		auto global_term_info_it = global_lexicon.find(*q_term_it);
		if (global_term_info_it == global_lexicon.end()) // IMPOSSIBLE!
			abort();

		// Create 'n load posting list's info into vector
		const auto& [term, posting_info] = *posting_info_it;
		auto docid_decoder = docid_decoder_t(inverted_indices + posting_info.start_pos_docid, inverted_indices + posting_info.end_pos_docid);
		auto docid_decoder_begin = docid_decoder.begin();
		auto freq_decoder = freq_decoder_t(inverted_indices_freqs + posting_info.start_pos_freq, inverted_indices + posting_info.end_pos_freq);

		// Iterators will lose refs to their decoders, but that's ok bc they do not have references to them
		posting_lists_its.push_back({
			.docid_curr = docid_decoder_begin, .docid_end = docid_decoder.end(),
			.freq_cur = freq_decoder.begin(), .freq_end = freq_decoder.end(),

			// some term's information
			.document_freq = global_term_info_it->second,
			.idf = QueryTFIDFScorer::idf(n_docs, global_term_info_it->second)
		});

		n_docs_to_process = std::max(n_docs_to_process, posting_info.n_docs);
		docid_base = std::min(docid_base, (docid_t)*docid_decoder_begin);

		++q_term_it;
	}

	docid_t curr_docid = docid_base;

	// Iterate all documents 'til we exhaust them
	// O(|D| * |Q| * log(|K|))
	while(not posting_lists_its.empty())
	{
		score_t score = 0;

		// Score current document
		for(const auto& iterator : posting_lists_its)
		{
			if(*iterator.docid_curr != curr_docid)
				continue;

			doclen_t dl = doclen_required ? document_index[curr_docid - base_docid].lenght : 0;
			score += scorer.score(*iterator.freq_cur, iterator.idf, dl, avgdl);
		}

		// Push computed result in the results, only if our score is greater than worst scoring doc in results
		if(results.empty() or score > results.top().score)
		{
			results.emplace(curr_docid, score);

			// If necessary pop-out the worst scoring element
			if (results.size() > top_k)
				results.pop();
		}

		docid_t next_docid = UINT64_MAX;
		// Move iterators to current docid or (the next closest one)
		for(auto iterator_it = posting_lists_its.begin(); iterator_it != posting_lists_its.end(); )
		{
			auto& iterator = *iterator_it;
			while(iterator.docid_curr != iterator.docid_end and *iterator.docid_curr <= curr_docid)
				++iterator.docid_curr, ++iterator.freq_cur;

			// We exhausted this posting list, let's remove it
			if(iterator.docid_curr == iterator.docid_end)
			{
				iterator_it = posting_lists_its.erase(iterator_it);
				continue;
			}

			next_docid = std::min(next_docid, (docid_t)*iterator.docid_curr);

			// Continue
			++iterator_it;
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

}
