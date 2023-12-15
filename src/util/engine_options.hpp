#pragma once
#include <string>
#include <filesystem>

struct engine_options
{
	enum algorithm_t {DAAT_DISJUNCTIVE, DAAT_CONJUNCTIVE, BMM};
	enum score_t {BM25, TFIDF};

	unsigned k = 10;
	std::string run_name = "MIRCV0";
	algorithm_t algorithm = DAAT_DISJUNCTIVE;
	bool batch_mode = false;
	std::filesystem::path data_dir = "data";
	unsigned thread_count = 1;
	score_t score = BM25;

	engine_options(int argc, char **argv);
};

