#pragma once

#include <cstddef>
#include <unordered_map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <iterator>
#include "../index/types.hpp"
#include "../index/Index.hpp"
#include "../codes/variable_blocks.hpp"

namespace sindex
{

class IndexBuilder
{
private:
	const docid_t base_docid;
	const docid_t n_docs;

    // Structure containing all of the needful information about a posting list
    struct PostingList
    {
        std::vector<uint8_t> docids;
        std::vector<uint8_t> freqs;
        freq_t n_docs = 0;
    };

    // For each term in the lexicon we save two vectors: the first contains the compressed docIDs
    // the second one the compressed frequencies
    // term -> <compressedDocIDs[], compressedFreqs[]>
    std::map<std::string,PostingList> inverted_index;
    std::vector<DocumentInfo> document_index;

public:
	explicit IndexBuilder(docid_t n_docs, docid_t base = 0):
		base_docid(base), n_docs(n_docs), document_index(n_docs) {}

    /**
    * Function that adds a frequency of a term associated to a documentID
    * It is assumed to recieve strictly increasing docIDs 
    * @param term index of the inverted index
    * @param id docid
    * @param occurrences number of occurrences of the term
    */
    void add_to_post(const std::string& term, docid_t id, freq_t occurrences)
    {
        // Compute compressed representation of the docID and the frequency
        auto cDocId = codes::VariableBytes(id);
        auto cFreqs = codes::VariableBytes(occurrences);

        // Insert the compressed representation of the docID and the frequency in the inverted index
		auto& entry = inverted_index[term];
		entry.docids.insert(entry.docids.end(), cDocId.bytes, cDocId.bytes + cDocId.used_bytes);
		entry.freqs.insert(entry.freqs.end(), cFreqs.bytes, cFreqs.bytes + cFreqs.used_bytes);
		entry.n_docs += 1;
    }

    /**
    * Inserts the document length inside the document index
    * @param docid index of the document
    * @param doc struct containing the doc number and its length
    */
    void add_to_doc(const docid_t docid, const DocumentInfo& doc)
    {
		assert(docid >= base_docid);
        document_index[docid - base_docid] = doc;
    }

    void write_to_disk(std::ostream& docid_teletype, std::ostream& freq_teletype, std::ostream& lexicon_teletype, std::ostream& document_index_teletype);
};

}
