#include <string>
#include <vector>
#include <iostream>
#include <set>
#include <filesystem>
#include "normalizer/WordNormalizer.hpp"
#include "index/types.hpp"
#include "index/Index.hpp"
#include "index/query_scorer.hpp"
#include "util/memory.hpp"
#include "util/thread_pool.hpp"
#include "index_worker.hpp"
#include "util/engine_options.hpp"

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	// Parse command line options
	const engine_options options(argc, argv);

	// Check if data dir exists
	if(not std::filesystem::exists(options.data_dir))
	{
		std::cerr << "Data dir " << options.data_dir << " does not exist" << std::endl;
		return -1;
	}

	// Load all db stuff
	memory_mmap metadata_mem(options.data_dir/"metadata");
	memory_mmap global_lexicon_mem(options.data_dir/"global_lexicon");
	sindex::Index<>::global_lexicon_t global_lexicon(global_lexicon_mem);

	std::list<index_worker_t<sindex::SigmaLexiconValue>> indices;

	// VROOOOOM STANDARD IO
	std::ios_base::sync_with_stdio(false);

	// Select the scorer
	std::unique_ptr<sindex::QueryScorer> scorer = nullptr;
	switch (options.score)
	{
	case engine_options::BM25:
		scorer = std::make_unique<sindex::QueryBM25Scorer>();
		break;
	case engine_options::TFIDF:
		scorer = std::make_unique<sindex::QueryTFIDFScorer>();
		break;
	}

	// Find all folders in the folder, each folder is a doc-partitioned db
	for (auto const& dir_entry : std::filesystem::directory_iterator(options.data_dir))
	{
		if(not dir_entry.is_directory())
			continue;

		std::clog << "Loading index chunk from " << dir_entry.path() << std::endl;
		indices.emplace_back(dir_entry, metadata_mem, global_lexicon, *scorer, "lexicon");
	}

	std::string query;
	unsigned long q_id = 0;
	normalizer::WordNormalizer wn;

	// Where I store the results, one array's cell per worker, then I'll merge them
	std::unique_ptr<std::vector<sindex::result_t>[]> results(new std::vector<sindex::result_t>[indices.size()]);
	std::vector<sindex::result_t> merged_results;
	merged_results.reserve(options.k * indices.size() + 1);

	// Workers
	thread_pool tp(options.thread_count);

	// Leggi righe da stdin fintanto EOF
	auto read_interactive = [&]() -> bool {
		std::cout << ++q_id << ") Waiting for input: ";
		return (bool)std::getline(std::cin, query);
	};
	auto read_batch = [&]() -> bool { return std::cin >> q_id and std::getline(std::cin, query); };
	while (options.batch_mode ? read_batch() : read_interactive())
	{
		if(query.empty())
			continue;

		// bench stuff
		const auto start_time = std::chrono::steady_clock::now();

		// Tokenizza la query
		std::set<std::string> tokens;
		auto terms = wn.normalize(query);
		while(true)
		{
			const auto& term = terms.next();
			if(term.empty())
				break;
			tokens.insert(term);
		}

		// Solve the query
		for(auto i = 0; auto& index : indices)
		{
			tp.add_job([&, pos = i++] {
				switch (options.algorithm)
				{
				case engine_options::DAAT_DISJUNCTIVE:
					results[pos] = index.index.query(tokens, false, options.k);
					break;
				case engine_options::DAAT_CONJUNCTIVE:
					results[pos] = index.index.query(tokens, true, options.k);
					break;
				case engine_options::BMM:
					results[pos] = index.index.query_bmm(tokens, options.k);
					break;
				}
			});
		}

		tp.wait_all_jobs();

		// Merge them all
		for(size_t i = 0; i < indices.size(); ++i)
			merged_results.insert(merged_results.end(), results[i].begin(), results[i].end());

		std::sort(merged_results.begin(), merged_results.end(), std::greater<>());
		if(merged_results.size() > options.k)
			merged_results.resize(options.k); // top-k results

		const auto stop_time = std::chrono::steady_clock::now();
		auto& out_tty = options.batch_mode ? std::clog : std::cout;
		out_tty << "Solved query " << q_id << " in " << (stop_time - start_time) / 1.0ms << "ms" << std::endl;

		for (size_t i = 0; i < merged_results.size(); ++i)
			//if(not merged_results[i].docno.empty())
			std::cout << q_id<< " Q0 " << merged_results[i].docno
				<< " " << (i+1) << " " << merged_results[i].score << " " << options.run_name << std::endl;

		merged_results.clear();
	}
	return 0;
}
