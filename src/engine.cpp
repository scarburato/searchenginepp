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

static sindex::QueryTFIDFScorer tfidf_scorer;

struct index_worker_t
{
	memory_mmap local_lexicon_mem;
	sindex::Index::local_lexicon_t local_lexicon;

	memory_mmap iid_mem;
	memory_mmap iif_mem;
	memory_mmap di_mem;

	sindex::Index index;

	index_worker_t(const std::filesystem::path& db, memory_area& metadata, sindex::Index::global_lexicon_t& global_lexicon):
			local_lexicon_mem(db/"lexicon"),
			local_lexicon(local_lexicon_mem),
			iid_mem(db/"posting_lists_docids"),
			iif_mem(db/"posting_lists_freqs"),
			di_mem(db/"document_index"),
			index(std::move(local_lexicon), global_lexicon, iid_mem, iif_mem, di_mem, metadata, tfidf_scorer)
	{}
};

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	const std::filesystem::path in_dir = argc > 1 ? argv[1] : "data";
	if(not std::filesystem::exists(in_dir))
		abort();

	// Load all db stuff
	memory_mmap metadata_mem(in_dir/"metadata");
	memory_mmap global_lexicon_mem(in_dir/"global_lexicon");
	sindex::Index::global_lexicon_t global_lexicon(global_lexicon_mem);

	std::vector<index_worker_t> indices;
	indices.reserve(100); // @TODO @FIXME SOLVE THIS REFERENCE MADNESS

	// Find all folders in the folder, each folder is a doc-partitioned db
	for (auto const& dir_entry : std::filesystem::directory_iterator(in_dir))
	{
		if(not dir_entry.is_directory())
			continue;

		std::cout << dir_entry.path() << std::endl;
		indices.emplace_back(dir_entry, metadata_mem, global_lexicon);
	}

	std::string query;
	normalizer::WordNormalizer wn;

	// Leggi righe da stdin fintanto EOF
	while (std::getline(std::cin, query))
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

		thread_pool tp(1);

		for(auto& index : indices)
		{
			tp.add_job([&] {
				auto results = index.index.query(tokens);
				for(const auto& res : results)
					std::cout << res.docno << ',' << res.score << '\t';
				std::cout << std::endl;
			});
		}

		tp.wait_all_jobs();

		const auto stop_time = std::chrono::steady_clock::now();
		std::cout << "Solved in " << (stop_time - start_time) / 1.0ms << "ms" << std::endl;
	}
	return 0;
}
