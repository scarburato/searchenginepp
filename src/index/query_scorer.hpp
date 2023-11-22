#pragma once
#include "types.hpp"

namespace sindex
{

class QueryScorer
{
public:
	virtual ~QueryScorer() = default;

	virtual score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) = 0;
	virtual bool needs_doc_metadata() {return false;}
};

class QueryTFIDFScorer: public QueryScorer
{
public:
	score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) override;
	static score_t idf(size_t n_docs, freq_t df_term);
};

class QueryBM25Scorer: public QueryScorer
{
	double k1;
	double b;

public:
	explicit QueryBM25Scorer(double k1 = 1, double b = 1);
	score_t score(freq_t tf_term_doc, score_t idf, doclen_t dl, double avgdl) override;
	bool needs_doc_metadata() override {return true;}
};

}
