#pragma once
#include "types.hpp"

namespace sindex
{

class QueryScorer
{
public:
	virtual ~QueryScorer() = default;

	virtual score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) const = 0;
	virtual bool needs_doc_metadata() const {return false;}
	virtual score_t get_sigma(const SigmaLexiconValue& lv) const = 0;
	virtual score_t get_sigma(const SigmaLexiconValue::skip_pointer_t& skip_ptr) const = 0;
};

class QueryTFIDFScorer: public QueryScorer
{
public:
	score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) const override;
	static score_t idf(size_t n_docs, freq_t df_term);
	score_t get_sigma(const SigmaLexiconValue& lv) const override;
	score_t get_sigma(const SigmaLexiconValue::skip_pointer_t& skip_ptr) const override;
};

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
