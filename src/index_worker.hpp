#pragma once
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

template<class LVT>
struct index_worker_t
{
	memory_mmap local_lexicon_mem;
	typename sindex::Index<LVT>::local_lexicon_t local_lexicon;

	memory_mmap iid_mem;
	memory_mmap iif_mem;
	memory_mmap di_mem;

	sindex::Index<LVT> index;

	index_worker_t(const std::filesystem::path& db, memory_area& metadata, typename sindex::Index<LVT>::global_lexicon_t& global_lexicon, sindex::QueryScorer& scorer, const std::string& lexicon_name = "lexicon_temp"):
			local_lexicon_mem(db/lexicon_name),
			local_lexicon(local_lexicon_mem),
			iid_mem(db/"posting_lists_docids"),
			iif_mem(db/"posting_lists_freqs"),
			di_mem(db/"document_index"),
			index(std::move(local_lexicon), global_lexicon, iid_mem, iif_mem, di_mem, metadata, scorer)
	{}
};
