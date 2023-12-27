#include <cmath>
#include "query_scorer.hpp"

namespace sindex
{
/*
   The 'tab64' array holds precomputed values for fast calculation of log2_64.
   Each value in the array represents the index of the highest set bit in a 6-bit binary number.
   This array allows quick lookup for log2_64 computation by using the index as the input.
   For instance, if the input is '4', the value in tab64[4] = 22, which means log2_64(4) = 22.
*/
static const int tab64[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5};

/*
   The function 'log2_64' calculates the logarithm base 2 of a 64-bit unsigned integer value.
   The function uses bitwise operations to find the position of the highest set bit in 'value'.

   The steps include setting all bits below the highest set bit to 1 and then indexing into 'tab64'
   to find the position of the highest bit, which corresponds to log2 of the value.

   Example:
   For instance, if 'value' is 16 (binary 10000), the highest set bit is at position 4 (from the right).
   The function will return tab64[4] = 22, indicating log2_64(16) = 22.
*/
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

/*
   The method 'score' calculates the TF-IDF score for a term in a document.

   Parameters:
   - 'tf_term_doc': The term frequency in the document.
   - 'idf': The inverse document frequency of the term.
   - 'dl': The document length.
   - 'avgdl': The average document length in the corpus.

   Explanation:
   - If 'tf_term_doc' is zero, indicating the term doesn't appear in the document, the method returns 0.
   - Depending on the preprocessor macro 'USE_FAST_LOG', the method uses either a fast log function 'log2_64'
     or the standard 'log2' function to calculate the logarithm base 2 of 'tf_term_doc'.
   - It then calculates the TF-IDF score using the formula (1 + log2(tf_term_doc)) * idf and returns the result.
*/
score_t QueryTFIDFScorer::score(freq_t tf_term_doc, score_t idf, [[maybe_unused]] doclen_t dl, [[maybe_unused]] double avgdl) const
{
	if (tf_term_doc == 0)
		return 0;

#ifdef USE_FAST_LOG
	return (1+ log2_64(tf_term_doc)) * idf;
#else
	return (1 + log2(tf_term_doc)) * idf;
#endif
}

/*
   The 'idf' method calculates the IDF of a term.

   Parameters:
   - 'n_docs': Total number of documents in the corpus.
   - 'df_term': Document frequency of the term.

   Explanation:
   - It computes the IDF using the formula log2(n_docs / df_term), where 'n_docs' is the total number of documents
     and 'df_term' is the document frequency of the term.
   - The method returns the calculated IDF.
*/
score_t QueryTFIDFScorer::idf(size_t n_docs, freq_t df_term)
{
	return log2((double) n_docs / (double) df_term);
}
/*
   The 'get_sigma' method retrieves the TF-IDF sigma value associated with a given 'SigmaLexiconValue'.

   Parameters:
   - 'lv': Reference to a 'SigmaLexiconValue' object.

   Explanation:
   - It fetches and returns the TF-IDF sigma value stored within the 'SigmaLexiconValue' object ('lv.tfidf_sigma').
*/
score_t QueryTFIDFScorer::get_sigma(const SigmaLexiconValue &lv) const
{
	return lv.tfidf_sigma;
}
/*
   The 'get_sigma' method retrieves the TF-IDF upper bound sigma value associated with a given skip pointer 'skip_ptr'.

   Parameters:
   - 'skip_ptr': Reference to a 'SigmaLexiconValue::skip_pointer_t' object.

   Explanation:
   - It fetches and returns the TF-IDF upper bound sigma value stored within the skip pointer ('skip_ptr.tfidf_ub').
*/
score_t QueryTFIDFScorer::get_sigma(const SigmaLexiconValue::skip_pointer_t &skip_ptr) const
{
	return skip_ptr.tfidf_ub;
}

QueryBM25Scorer::QueryBM25Scorer(double k1, double b) : k1(k1), b(b)
{}

/*
   The 'score' method calculates the BM25 score for a given term within a document.

   Parameters:
   - 'tf_term_doc': The frequency of the term in the document.
   - 'idf': The IDF of the term.
   - 'dl': The length of the document.
   - 'avgdl': The average document length.

   Calculation:
   - Calculates the BM25 score using the formula:
     BM25 Score = (tf_term_doc / (k1 * ((1 - b) + b * (dl / avgdl)) + tf_term_doc)) * idf

   Explanation:
   - The BM25 score is computed based on the frequency of the term within the document ('tf_term_doc'), the IDF of the term ('idf'),
     the document length ('dl'), and the average document length ('avgdl').
   - The 'k1' and 'b' values are constants controlling the term frequency saturation and length normalization, respectively.
*/
score_t QueryBM25Scorer::score(sindex::freq_t tf_term_doc, sindex::score_t idf, sindex::doclen_t dl, double avgdl) const
{
	return (tf_term_doc / (k1*((1-b) + b*dl/avgdl) + tf_term_doc)) * idf;
}

/*
   The 'get_sigma' method returns the BM25 sigma value associated with a SigmaLexiconValue.

   Parameter:
   - 'lv': A reference to a SigmaLexiconValue object.

   Return:
   - Returns the BM25 sigma value from the provided 'lv' object.

   Explanation:
   - This method retrieves and returns the BM25 sigma value associated with a SigmaLexiconValue object.
   - The BM25 sigma value is part of the SigmaLexiconValue structure.
*/
score_t QueryBM25Scorer::get_sigma(const SigmaLexiconValue &lv) const
{
	return lv.bm25_sigma;
}

/*
   The 'get_sigma' method retrieves the BM25 upper bound value associated with a skip pointer.

   Parameter:
   - 'skip_ptr': A reference to a skip pointer (a member of SigmaLexiconValue).

   Return:
   - Returns the BM25 upper bound value (bm25_ub) from the provided skip pointer.

   Explanation:
   - This method is designed to extract and return the BM25 upper bound value associated with a skip pointer in a SigmaLexiconValue structure.
   - It accesses the 'bm25_ub' value stored within the skip pointer.
*/
score_t QueryBM25Scorer::get_sigma(const SigmaLexiconValue::skip_pointer_t &skip_ptr) const
{
	return skip_ptr.bm25_ub;
}


}
