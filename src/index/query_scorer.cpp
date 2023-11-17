#include <cmath>
#include "query_scorer.hpp"

sindex::score_t sindex::QueryTFIDFScorer::score(sindex::freq_t tf_term_doc, sindex::freq_t df_t, size_t n_docs)
{
	if(tf_term_doc == 0)
		return 0;

	return (1 + log(tf_term_doc)) * log((double)n_docs / (double)df_t);
}
