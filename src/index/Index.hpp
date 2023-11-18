#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../codes/diskmap/diskmap.hpp"
#include "../codes/variable_blocks.hpp"
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
private:
	using local_lexicon_t = codes::disk_map<LexiconValue>;
	using global_lexicon_t = codes::disk_map<freq_t>;

	local_lexicon_t& local_lexicon;
	global_lexicon_t& global_lexicon;

	const uint8_t *inverted_indices;
	size_t inverted_indices_length;

	const uint8_t *inverted_indices_freqs;
	size_t inverted_indices_freqs_length;

	std::vector<DocumentInfo> document_index;

	QueryScorer& scorer;
public:
	/**
	 * @param lx the local lexicon
	 * @param gx the global lexicon
	 * @param iid inverted indices
	 * @param iif inverted indices
	 * @param di document index
	 */
	Index(local_lexicon_t& lx, global_lexicon_t& gx, const memory_area& iid, const memory_area& iif,
		  const memory_area& di, QueryScorer& qs);
	~Index();

	void set_scorer(QueryScorer& qs) {scorer = qs;}
	std::vector<result_t> query(std::set<std::string>& query, size_t top_k = 10);
};

}
