#pragma once
#include "types.hpp"

namespace sindex
{

class QueryScorer
{
public:
	virtual ~QueryScorer() {};

	virtual score_t score(freq_t tf_term_doc, score_t idf) = 0;
};

class QueryTFIDFScorer: public QueryScorer
{
public:
	score_t score(freq_t tf_term_doc, score_t idf) override;
	static score_t idf(size_t n_docs, freq_t df_term);
};

}
