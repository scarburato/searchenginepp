#pragma once
#include "types.hpp"

namespace sindex
{

class QueryScorer
{
public:
	virtual ~QueryScorer() = 0;

	virtual score_t score(freq_t tf_term_doc, freq_t df_t, size_t n_docs) = 0;
};

class QueryTFIDFScorer: public QueryScorer
{
	score_t score(sindex::freq_t tf_term_doc, sindex::freq_t df_t, size_t n_docs) override;
};

}
