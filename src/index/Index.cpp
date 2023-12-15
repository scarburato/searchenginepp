#include <iterator>
#include <memory>
//#include <iostream>
#include "Index.hpp"
#include "types.hpp"

namespace sindex
{

template<>
const SigmaLexiconValue::skip_pointer_t& Index<SigmaLexiconValue>::PostingList::get_block(const iterator& it) const
{
	auto block_it = lv.skip_pointers.begin();

	// Find the block that contains the current iterator
	while(block_it->last_docid < it->first)
		++block_it;

	return *block_it;
}

template<>
std::vector<result_t> Index<SigmaLexiconValue>::query_bmm(std::set<std::string> &query, size_t top_k)
{
	// Top-K results. This is a min queue (for that we use std::greater, of course), so that the minimum element can
	// be popped
	pending_results_t results;
	auto [posting_lists_its, min_docid] = build_helpers(query);
	docid_t curr_docid = min_docid;
	size_t pivot = 0;
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
	while(pivot < posting_lists_its.size() and not posting_lists_its.empty())
	{
		score_t score = 0.0;
		docid_t next = DOCID_MAX;

		// Move the iterator to (p + pivot)
		auto p_it = posting_lists_its.begin();
		for(size_t i = 0; i < pivot; ++i)
			++p_it;

		for(auto i = pivot; i < posting_lists_its.size(); ++i, ++p_it)
		{
			if(p_it->it->first == curr_docid)
			{
				score += p_it->pl.score(p_it->it, scorer);
				++p_it->it;
			}
			
			next = std::min(next, p_it->it->first);
		}

		if(pivot != 0 and score + upper_bounds[pivot - 1] > θ)
		{
			auto p_it = posting_lists_its.begin();
			std::vector<score_t> bub(pivot);

			// Populate the bub's array
			bub[0] = p_it->pl.get_block(p_it->it).bm25_ub;
			for(size_t i = 1; i < pivot; ++i)
			{
				++p_it;
				bub[i] = bub[i - 1] + p_it->pl.get_block(p_it->it).bm25_ub;
			}
			
			for(size_t j = 0; j < pivot; ++j)
			{
				size_t i = pivot - j - 1;
				if(score + bub[i] < θ)
					break;

				// Move to next posting @todo @fixme use the damn skip list to GEQ
				p_it->it.nextGEQ(curr_docid);
				if(p_it->it != p_it->pl.end() and p_it->it->first == curr_docid)
					score += p_it->pl.score(p_it->it, scorer);
			}
		}

		// Push computed result in the results, only if our score is greater than worst scoring doc in results
		if(results.empty() or score > results.top().score)
		{
			results.emplace(curr_docid, score);

			// If necessary pop-out the worst scoring element
			if (results.size() > top_k)
				results.pop();

			θ = results.top().score;

			while (pivot < posting_lists_its.size() and upper_bounds[pivot] <= θ)
				++pivot;
		}

		// Removed the exasthed posting lists
		size_t j = 0;
		for(auto p_it = posting_lists_its.begin(); p_it != posting_lists_its.end();)
		{
			// We exhausted this posting list, let's remove it
			if(p_it->it == p_it->pl.end())
			{
				p_it = posting_lists_its.erase(p_it);

				// Shift the pivot if necessary, that is when 
				// we removed a posting list before the pivot
				if(pivot > j)
					--pivot;

				continue;
			}

			// Continue
			++p_it;
			++j;
		}

		curr_docid = next;
	}

	return convert_results(results, top_k);
}

template<>
Index<SigmaLexiconValue>::PostingList::iterator Index<SigmaLexiconValue>::PostingList::begin() const
{
	return {this, lv.skip_pointers.begin(), docid_dec.begin(), freq_dec.begin()};
}

template<>
Index<SigmaLexiconValue>::PostingList::iterator Index<SigmaLexiconValue>::PostingList::end() const
{
	return {this, lv.skip_pointers.end(), docid_dec.end(), freq_dec.end()};
}

template<>
Index<SigmaLexiconValue>::PostingList::iterator& Index<SigmaLexiconValue>::PostingList::iterator::operator++()
{
	++docid_curr;
	++freq_curr;

	// Parse
	if(*this != parent->end())
	{
		current.first = *docid_curr;
		current.second = *freq_curr;

		// Specialization: if we reached end of a block, we move to the next one
		if(current_block_it != parent->lv.skip_pointers.end() and current.first > current_block_it->last_docid)
			++current_block_it;
	}
	return *this;
}

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::nextG(sindex::docid_t docid)
{
	// Move to the next block until we find one that contains the docid
	while(current_block_it != parent->lv.skip_pointers.end() and current_block_it->last_docid < docid)
		skip_block();

	// Found block, now iterate until we required docid
	while(*this != parent->end() and current.first <= docid)
		++*this;
}

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::nextGEQ(sindex::docid_t docid)
{
	// Move to the next block until we find one that contains the docid
	while(current_block_it != parent->lv.skip_pointers.end() and current_block_it->last_docid < docid)
		skip_block();

	// Found block, now iterate until we required docid
	while(*this != parent->end() and current.first < docid)
		++*this;
}

template<>
void Index<SigmaLexiconValue>::PostingList::iterator::skip_block()
{
	// Move to the next block
	++current_block_it;

	// We reached the end!
	if(current_block_it == parent->lv.skip_pointers.end())
		return;
	//std::clog << "skipping block" << std::endl;

	docid_curr = parent->docid_dec.at(current_block_it->docid_offset);
	auto freq_off = codes::deserialize_bit_offset(current_block_it->freq_offset);
	freq_curr = parent->freq_dec.at(freq_off.first, freq_off.second);

	current.first = *docid_curr;
	current.second = *freq_curr;

	assert(current.first - parent->index->base_docid < parent->index->n_docs);
}
}
