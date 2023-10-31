#include <cstdint>
#include <ranges>
#include <iostream>
#include <bits/stdc++.h> 
#include <unordered_map>
#include "IndexBuilder.hpp"
#include "../index/types.hpp"
#include "../codes/unary.hpp"

namespace sindex
{

/**
* Function that writes to disk the inverted index
* @param docid_teletype stream in which the docids are saved
* @param freq_teletype stream in which the frequencies are saved
* @param lexicon_teletype stream in which the lexicon is saved
* @param document_index_teletype stream in which the document index is saved
*/
void IndexBuilder::write_to_disk(std::ostream& docid_teletype, std::ostream& freq_teletype, std::ostream& lexicon_teletype, std::ostream& document_index_teletype)
{
	// Write down all terms to disk, first we write the number of buckets necessary for the hashmap
	uint64_t buckets = inverted_index.bucket_count();
	lexicon_teletype.write((char*)&buckets, sizeof(uint64_t));
	for(const auto& [term, array] : inverted_index)
    {
		lexicon_teletype.write(term.c_str(), term.size());
        const freq_t ni = array.size();
        lexicon_teletype.write((char *)&ni, sizeof(freq_t));
    }

	lexicon_teletype.flush();

    // Write to the teletype the posting list and its relative entry in the lexicon
    for(const auto& [term, array] : inverted_index)
    {
        // Iterate through all docids and save the docids
        auto iter_docid = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.first;});

        // Encode the posting lists of the relative term using the variable bytes algorithm 
        codes::VariableBlocksEncoder doc_encoder(iter_docid.begin(), iter_docid.end());

        // Write starting offset of the docids
		const uint64_t start_pos = docid_teletype.tellp();
		lexicon_teletype.write((char*)&start_pos, sizeof(uint64_t));

        // Save docid posting list
        for(uint8_t byte : doc_encoder)
            docid_teletype.put(byte);

        // Save ending offset of the docids
		const uint64_t end_pos = docid_teletype.tellp();
		lexicon_teletype.write((char*)&end_pos, sizeof(uint64_t));
    }

	// Write to disk all the id postings
	docid_teletype.flush();

    for(const auto& [term, array] : inverted_index)
    {
        // Iterate through all docids and save the frequencies
        auto iter_freq = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.second;});

        // Encode the posting lists of the relative term using the unary algorithm 
        codes::UnaryEncoder freq_encoder(iter_freq.begin(), iter_freq.end());

		// Write starting offset of the docids
		const uint64_t start_pos = freq_teletype.tellp();
		lexicon_teletype.write((char*)&start_pos, sizeof(uint64_t));

        // Save frequencies
        for(uint8_t byte : freq_encoder)
            freq_teletype.put(byte);

		// Save ending offset of the docids
		const uint64_t end_pos = freq_teletype.tellp();
		lexicon_teletype.write((char*)&end_pos, sizeof(uint64_t));
    }

	// flush freqs 'n lexicon
	freq_teletype.flush();
	lexicon_teletype.flush();

    // Document index part, first we write the base
	document_index_teletype.write((char*)&base_docid, sizeof(docid_t));
    for(const auto& [docid, document] : document_index)
    {
        document_index_teletype.write((char*)&docid, sizeof(docid));
        document_index_teletype.write((char*)&document, sizeof(DocumentInfo));
    }

	// final flush
	document_index_teletype.flush();
}

}

