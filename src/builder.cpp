#include <atomic>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <thread>
#include "index/query_scorer.hpp"
#include "index/types.hpp"
#include "index_worker.hpp"
#include "normalizer/WordNormalizer.hpp"
#include "indexBuilder/IndexBuilder.hpp"
#include "util/thread_pool.hpp"
#include "codes/diskmap/diskmap.hpp"

typedef std::pair<sindex::docno_t, std::string> doc_tuple_t;

// Chunks' sizes
constexpr size_t MAX_CHUNK_SPACE = 675'000'000;
constexpr size_t SKIP_BLOCK_SIZE = 2'000;

std::atomic<sindex::doclen_t> global_doc_len_sum = 0;
std::vector<std::filesystem::path> index_folders_paths;

// At most one thread should write on disk at a given time
// it is also used to access the global vector with the lexicons' paths
std::mutex disk_writer_mutex;

/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */

static void process_chunk(std::shared_ptr<std::vector<doc_tuple_t>> chunk, sindex::docid_t base_id, size_t chunk_n, const std::filesystem::path& out_dir)
{
	using namespace std::chrono_literals;

	normalizer::WordNormalizer wn;
	sindex::IndexBuilder indexBuilder(chunk->size(), base_id);
	sindex::docid_t docid = base_id;
	sindex::doclen_t doc_len_sum = 0;

	const auto start_time = std::chrono::steady_clock::now();

	// Process all docs (lines) in a chunk
    for (const auto &line : *chunk)
	{

		// Extract all tokens and their freqs
		std::unordered_map<std::string, sindex::freq_t> term_freqs;
		auto terms = wn.normalize(line.second);

		while (true)
		{
			const auto &term = terms.next();
			if (term.empty())
				break;

			term_freqs[term]++;
		}

		auto doc_len = std::accumulate(term_freqs.begin(), 
												term_freqs.end(), (sindex::doclen_t)0,
												[](sindex::freq_t sum, const auto& pair) {
													return sum + pair.second;
					});
		// Add to index
		indexBuilder.add_to_doc(
				docid,
				{.docno = line.first, .lenght = doc_len}
		);

		doc_len_sum += doc_len;

		for (const auto &[term, freq]: term_freqs)
			indexBuilder.add_to_post(term, docid, freq);

		// Increment docid
		docid += 1;
	}


	// Retrieve n_docs_view from IndexBuilder
	// Wait for exclusive access to disk
	std::lock_guard<std::mutex> guard(disk_writer_mutex);

	global_doc_len_sum += doc_len_sum;
	const auto stop_time_proc = std::chrono::steady_clock::now();

	// Write stuff to disk
	std::string base_name = "db_" + std::to_string(chunk_n);
	if(not std::filesystem::exists(out_dir/base_name))
		std::filesystem::create_directory(out_dir/base_name);

	auto pl_docids = std::ofstream(out_dir/base_name/"posting_lists_docids", std::ios_base::binary);
	auto pl_freqs = std::ofstream(out_dir/base_name/"posting_lists_freqs", std::ios_base::binary);
	auto lexicon = std::ofstream(out_dir/base_name/"lexicon_temp", std::ios_base::binary);
	auto doc_index = std::ofstream(out_dir/base_name/"document_index", std::ios_base::binary);

	index_folders_paths.push_back(out_dir/base_name);

	indexBuilder.write_to_disk(pl_docids, pl_freqs, lexicon, doc_index);

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout
		<< "Chunk " << chunk_n
		<< " (thread " << std::hex << std::this_thread::get_id() << std::dec << " )"
		<< " processed in " << (stop_time_proc - start_time) / 1.0s << "s"
		<< " and written in " << (stop_time - stop_time_proc) / 1.0s << "s"
		<< " ( " << (stop_time - start_time)/1s << "s elapsed)" << std::endl;
}

void write_global_lexicon_to_disk_map(const std::filesystem::path& out_dir) {
	std::ofstream lexicon_teletype(out_dir / "global_lexicon", std::ios::binary);

	// Open up all files and maps from the local lexicon
	struct lexicon_temp {memory_mmap file; codes::disk_map<sindex::LexiconValue> lexicon;};
	std::vector<std::unique_ptr<lexicon_temp>> lexica;
	lexica.reserve(index_folders_paths.size());

	for(const auto& db_path : index_folders_paths)
	{
		memory_mmap lexicon_mmap(db_path/"lexicon_temp");

		lexica.emplace_back(
				std::make_unique<lexicon_temp>(
						std::move(lexicon_mmap), codes::disk_map<sindex::LexiconValue>(lexicon_mmap)));
	}

	// Preparing merge
	using iterator = codes::disk_map<sindex::LexiconValue>::iterator;
	std::vector<std::pair<iterator, iterator>> ranges;
	ranges.reserve(lexica.size());

	// Fill ranges
	for(const auto& lexicon : lexica)
		ranges.emplace_back(lexicon->lexicon.begin(), lexicon->lexicon.end());

	const auto filter_f = [](const sindex::LexiconValue& v) -> sindex::freq_t {return v.n_docs;};
	const auto merge_f = []([[maybe_unused]] const std::string& key, const std::vector<sindex::freq_t>& values) {
		return std::accumulate(values.begin(), values.end(), (sindex::freq_t)0);
	};

	// Merge
	codes::merge<sindex::freq_t, iterator, codes::BLOCK_SIZE, sindex::LexiconValue>(lexicon_teletype, ranges, merge_f, filter_f);
	lexicon_teletype.flush();
}

void write_metadata(const std::filesystem::path& out_dir, const size_t ndocs) {
	std::ofstream metadata(out_dir / "metadata", std::ios::binary);
	metadata.write((char*)&global_doc_len_sum, sizeof(sindex::doclen_t));
	metadata.write((char*)&ndocs, sizeof(size_t));
}

/**
 * This function computes the sigma for each term in the local lexicon and writes it to disk
 * It [will] also computes the skipping list. @todo 
 * @param dir the directory where the index is stored
 */
void write_sigma_lexicon(const std::filesystem::path& dir) {
	// Load all db stuff
	memory_mmap metadata_mem(dir/".."/"metadata");
	memory_mmap global_lexicon_mem(dir/".."/"global_lexicon");
	sindex::Index::global_lexicon_t global_lexicon(global_lexicon_mem);
	sindex::QueryTFIDFScorer tfidf_scorer;
	sindex::QueryBM25Scorer bm25_scorer;

	index_worker_t index_worker(dir, metadata_mem, global_lexicon, tfidf_scorer);

	std::ofstream sigma_lexicon(dir/"lexicon", std::ios::binary);

	codes::disk_map_writer<sindex::SigmaLexiconValue> sigma_lexicon_writer(sigma_lexicon);

	for(const auto& [term, lv] : index_worker.index.get_local_lexicon())
	{
		sindex::SigmaLexiconValue slv = lv;
		sindex::SigmaLexiconValue::skip_pointer_t current_skip;
		size_t i = 1;

		auto pl = index_worker.index.get_posting_list(term, lv);
		auto pl_it = pl.begin();
		auto pl_curr_block_off = pl.get_offset(pl_it);

		// For each posting we score it and update the sigma, if necessary
		for(; pl_it != pl.end(); ++pl_it, ++i)
		{
			const auto& [docid, freq] = *pl_it;

			auto tfidf_score = pl.score(pl_it, tfidf_scorer);
			auto bm25_score = pl.score(pl_it, bm25_scorer);


			slv.tfidf_sigma = std::max(slv.tfidf_sigma, tfidf_score);
			current_skip.tfidf_ub = std::max(current_skip.tfidf_ub, tfidf_score);

			slv.bm25_sigma = std::max(slv.bm25_sigma, bm25_score);
			current_skip.bm25_ub = std::max(current_skip.bm25_ub, bm25_score);
			
			// If we reached the end of the block
			if (i % SKIP_BLOCK_SIZE == 0)
			{
				current_skip.last_docid = docid;
				current_skip.docid_offset = pl_curr_block_off.docid_off;
				current_skip.freq_offset = pl_curr_block_off.freq_off;
				slv.skip_pointers.push_back(current_skip);

				current_skip = {};
				pl_curr_block_off = pl.get_offset(pl_it + 1);
			}
		}

		// Write the new value
		sigma_lexicon_writer.add(term, slv);
	}
	sigma_lexicon_writer.finalize();
}

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	size_t space_count = 0;

	std::string pid_str, doc;
    size_t line_count = 1;
	sindex::docid_t docid_start = 1;

	// This is where we'll store the output stuff
	const std::filesystem::path out_dir = argc > 1 ? argv[1] : "data";
	if(std::filesystem::exists(out_dir))
		std::filesystem::remove_all(out_dir);

	std::filesystem::create_directory(out_dir);

	// VROOOOOM STANDARD IO
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	// A chunk contains a partition of lines read from stdin to be processed
	// We store them in an array of <docno, doc> that will be deleted by
	// process_chunk
	auto chunk = std::make_shared<std::vector<doc_tuple_t>>();
	size_t chunk_n = 0;

	// bench stuff
	const auto start_time = std::chrono::steady_clock::now();

	// Multi-thread
	thread_pool pool(4);

	// Iterate through all lines from STDIN
    while (std::getline(std::cin, pid_str, '\t') and std::getline(std::cin, doc))
    {
        chunk->emplace_back(pid_str, doc);

		// Compute the space occupied by doc and pid_str
		space_count += pid_str.size() + doc.size();
		// Send the chunk only if overcomes the space's threshold
		if (space_count >= MAX_CHUNK_SPACE)
		{
			pool.add_job([chunk = std::move(chunk), docid_start, chunk_n, out_dir] {
				process_chunk(chunk, docid_start, chunk_n, out_dir);
			});
			chunk_n += 1;
			docid_start = line_count + 1;

			// Allocate new chunk for next round
			chunk = std::make_shared<std::vector<doc_tuple_t>>();

			// Reset space
			space_count = 0;

			// Wait for a spot before we continue
			pool.wait_for_free_worker();
		}

		line_count++;
	}

    // Process the remaining lines which are less than CHUNK_SIZE
	if (not chunk->empty())
	{
		pool.add_job([chunk = std::move(chunk), docid_start, chunk_n, out_dir]() {
			process_chunk(chunk, docid_start, chunk_n, out_dir);
		});
		chunk_n += 1;
	}

	pool.wait_all_jobs();

	const auto stop_time_1 = std::chrono::steady_clock::now();
	std::cout << "Indices built in " << (stop_time_1 - start_time) / 1.0s << "s" << std::endl;

	// Write global_lexicon using disk_map_writer
	write_global_lexicon_to_disk_map(out_dir);
	write_metadata(out_dir, line_count - 1);

	const auto stop_time_2 = std::chrono::steady_clock::now();
	std::cout << "Built global lexicon from local lexica in " << (stop_time_2 - stop_time_1) / 1.0ms << "ms" << std::endl;

	// Create skip-list and compute their sigma
	for(const auto& path : index_folders_paths)
	{
		pool.wait_for_free_worker();
		pool.add_job([path]() {
			write_sigma_lexicon(path);
			std::cout << "(thread " << std::hex << std::this_thread::get_id() << std::dec << " )\t"
					  << "Built skipping list and sigmas for " << path << std::endl;
		});
	}
	pool.wait_all_jobs();

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout << "Re-built local lexica in " << (stop_time - stop_time_2) / 1.0s << "s" << std::endl;

	std::cout << "Processed " << (line_count - 1) << " documents in " << (stop_time - start_time) / 1.0s << "s"
		<< " " << (chunk_n) << " indices generated\n";

    return 0;
}
