#include <cmath>
#include "query_scorer.hpp"

namespace sindex
{

static const int tab64[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5};

// Fast integer log2
[[maybe_unused]] static int log2_64 (uint64_t value)
{
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

score_t QueryTFIDFScorer::score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl)
{
	if (tf_term_doc == 0)
		return 0;

#ifdef USE_FAST_LOG
	return (1+ log2_64(tf_term_doc)) * idf;
#else
	return (1 + log2(tf_term_doc)) * idf;
#endif
}

score_t QueryTFIDFScorer::idf(size_t n_docs, freq_t df_term)
{
	return log2((double) n_docs / (double) df_term);
}

QueryBM25Scorer::QueryBM25Scorer(double k1, double b) : k1(k1), b(b)
{}

score_t QueryBM25Scorer::score(sindex::freq_t tf_term_doc, sindex::score_t idf, sindex::doclen_t dl, double avgdl)
{
	return (tf_term_doc / k1*((1-b) + b*dl/avgdl) + tf_term_doc) * idf;
}


}
