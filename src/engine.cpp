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

static sindex::QueryTFIDFScorer tfidf_scorer;
static sindex::QueryBM25Scorer bm25_scorer;

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	const std::filesystem::path in_dir = argc > 1 ? argv[1] : "data";
	if(not std::filesystem::exists(in_dir))
		abort();

	// Load all db stuff
	memory_mmap metadata_mem(in_dir/"metadata");
	memory_mmap global_lexicon_mem(in_dir/"global_lexicon");
	sindex::Index<sindex::SigmaLexiconValue>::global_lexicon_t global_lexicon(global_lexicon_mem);

	std::list<index_worker_t<sindex::SigmaLexiconValue>> indices;

	// VROOOOOM STANDARD IO
	std::ios_base::sync_with_stdio(false);

	// Find all folders in the folder, each folder is a doc-partitioned db
	for (auto const& dir_entry : std::filesystem::directory_iterator(in_dir))
	{
		if(not dir_entry.is_directory())
			continue;

		std::clog << dir_entry.path() << std::endl;
		indices.emplace_back(dir_entry, metadata_mem, global_lexicon, bm25_scorer, "lexicon");
	}

	std::string query;
	unsigned long q_id;
	normalizer::WordNormalizer wn;

	// Where I store the results, one array's cell per worker, then I'll merge them
	std::unique_ptr<std::vector<sindex::result_t>[]> results(new std::vector<sindex::result_t>[indices.size()]);
	std::vector<sindex::result_t> merged_results;
	merged_results.reserve(10*indices.size() + 1);

	// Workers
	thread_pool tp(1);

	// Leggi righe da stdin fintanto EOF
	while (std::cin >> q_id and std::getline(std::cin, query))
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
		auto i = 0;
		for(auto& index : indices)
		{
			tp.add_job([&, pos = i++] {
				results[pos] = index.index.query(tokens);
			});
		}

		tp.wait_all_jobs();

		// Merge them all
		for(size_t i = 0; i < indices.size(); ++i)
			merged_results.insert(merged_results.end(), results[i].begin(), results[i].end());

		std::sort(merged_results.begin(), merged_results.end(), std::greater<>());
		merged_results.resize(10); // top-10 results

		const auto stop_time = std::chrono::steady_clock::now();
		std::clog << "Solved query " << q_id << " in " << (stop_time - start_time) / 1.0ms << "ms" << std::endl;

		for (size_t i = 0; i < merged_results.size(); ++i)
			if(not merged_results[i].docno.empty())
				std::cout << q_id<< " Q0 " << merged_results[i].docno
					<< " " << (i+1) << " " << merged_results[i].score << " MIRCV0" << std::endl;

		merged_results.clear();
	}
	return 0;
}
