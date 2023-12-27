#pragma once
#include "types.hpp"

namespace sindex
{

class QueryScorer
{
public:
	// - '= default;' specifies that the default implementation of the destructor is used.
	virtual ~QueryScorer() = default; // destructor

	virtual score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) const = 0;
	virtual bool needs_doc_metadata() const {return false;}
	virtual score_t get_sigma(const SigmaLexiconValue& lv) const = 0;
	virtual score_t get_sigma(const SigmaLexiconValue::skip_pointer_t& skip_ptr) const = 0;
};

/**
 * Definition of the QueryTFIDFScorer class, derived from the base class QueryScorer.
 *
 * The QueryTFIDFScorer class inherits from the QueryScorer base class
 *
 * The class provides the following public member functions:
 * - 'score': Calculates and returns the score based on provided parameters.
 * - 'static score_t idf': Computes and returns the inverse document frequency.
 * - 'get_sigma': Overrides a method from the base class to calculate and return a sigma value.
 * - 'get_sigma' (overload): Overrides another method from the base class to calculate and return a sigma value.
 *
 * These functions are responsible for scoring calculations, inverse document frequency computation,
 * and sigma value retrieval based on specific parameters.
 *
 */
class QueryTFIDFScorer: public QueryScorer
{
public:
	score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) const override;
	static score_t idf(size_t n_docs, freq_t df_term);
	score_t get_sigma(const SigmaLexiconValue& lv) const override;
	score_t get_sigma(const SigmaLexiconValue::skip_pointer_t& skip_ptr) const override;
};


/**
 * Definition of the QueryBM25Scorer class, derived from the base class QueryScorer.
 *
 * The QueryBM25Scorer class inherits from the QueryScorer base class, implying it shares
 * common functionalities and can extend or override its behavior.
 *
 * The class contains private member variables:
 * - 'k1' and 'b'
 *
 * The 'explicit' constructor initializes 'k1' and 'b' with default values if not explicitly provided.
 * The 'score' function computes and returns the score based on specified input parameters.
 * The 'needs_doc_metadata' function indicates whether document metadata is required for the scorer.
 * The 'get_sigma' functions override methods from the base class to calculate and return sigma values.
 *
 */
class QueryBM25Scorer: public QueryScorer
{
	double k1;
	double b;

public:
	explicit QueryBM25Scorer(double k1 = 0.82, double b = 0.68);
	score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) const override;
	bool needs_doc_metadata() const override {return true;}
	score_t get_sigma(const SigmaLexiconValue& lv) const override;
	score_t get_sigma(const SigmaLexiconValue::skip_pointer_t& skip_ptr) const override;
};

}
