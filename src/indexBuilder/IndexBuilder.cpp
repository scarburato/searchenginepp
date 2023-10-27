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

// Struct containing the offset of the inverted index associated with a term
struct LexiconInfo
{
	uint64_t docid_start_offset;
    uint64_t docid_end_offset;
    uint64_t freq_start_offset;
    uint64_t freq_end_offset;
};

/**
* Function that writes to disk the inverted index
* @param docid_teletype stream in which the docids are saved
* @param freq_teletype stream in which the frequencies are saved
* @param lexicon_teletype stream in which the lexicon is saved
* @param document_index_teletype stream in which the document index is saved
*/
void IndexBuilder::write_to_disk(std::ostream& docid_teletype, std::ostream& freq_teletype, std::ostream& lexicon_teletype, std::ostream& document_index_teletype)
{

    // Create a temporary map to save the informations about the lexicon
    std::unordered_map<std::string, LexiconInfo> lexicon;

    // Write to the teletype the posting list and its relative entry in the lexicon
    for(const auto& [term, array] : inverted_index)
    {
        // Iterate through all docids and save the docids
        auto iter_docid = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.first;});

        // Encode the posting lists of the relative term using the variable bytes algorithm 
        codes::VariableBlocksEncoder doc_encoder(iter_docid.begin(), iter_docid.end());

        // Save starting offset of the docids
        lexicon[term].docid_start_offset = docid_teletype.tellp();

        // Save docid posting list
        for(uint8_t byte : doc_encoder)
            docid_teletype.put(byte);

        // Save ending offset of the docids
        lexicon[term].docid_end_offset = docid_teletype.tellp();
    }

    for(const auto& [term, array] : inverted_index)
    {
        // Iterate through all docids and save the frequencies
        auto iter_freq = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.second;});

        // Encode the posting lists of the relative term using the unary algorithm 
        codes::UnaryEncoder freq_encoder(iter_freq.begin(), iter_freq.end());

        // Save starting offset of the frequencies
        lexicon[term].freq_start_offset = freq_teletype.tellp();

        // Save frequencies
        for(uint8_t byte : freq_encoder)
            freq_teletype.put(byte);

        // Save starting offset of the frequencies
        lexicon[term].freq_end_offset = freq_teletype.tellp();
    }

    // Write term offset informations stored inside the temporary lexicon map
    for(const auto& [term, info] : lexicon)
    {
        // Write the lexicon in the correspondent stream
        lexicon_teletype.write(term.c_str(), term.size());
        lexicon_teletype.write((char*)&info.docid_start_offset, sizeof(uint64_t));
        lexicon_teletype.write((char*)&info.docid_end_offset, sizeof(uint64_t));
        lexicon_teletype.write((char*)&info.freq_start_offset, sizeof(uint64_t));
        lexicon_teletype.write((char*)&info.freq_end_offset, sizeof(uint64_t));
    }

    // Document index part
    for(const auto& [docid, document] : document_index)
    {
        document_index_teletype.write((char*)&docid, sizeof(docid));
        document_index_teletype.write((char*)&document, sizeof(DocumentInfo));
    }
}

}

