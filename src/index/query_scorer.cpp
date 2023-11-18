#include <cmath>
#include "query_scorer.hpp"

namespace sindex
{

score_t QueryTFIDFScorer::score(freq_t tf_term_doc, freq_t df_term, size_t n_docs)
{
	if (tf_term_doc == 0)
		return 0;

	return (1 + log2(tf_term_doc)) * log2((double) n_docs / (double) df_term);
}

}
