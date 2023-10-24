#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include "../index/types.hpp"
#include "../codes/variable_blocks.hpp"

namespace index
{

class IndexBuilder
{
    private:

    std::map<std::string, std::vector<std::pair<docid_t, freq_t>>> term_to_inverted_index;

    public:

    /*
    * Function that adds a frequency of a term associated to a documentID
    * It is assumed to recieve strictly increasing docIDs 
    */
    void add(const std::string& term, docid_t id, freq_t occurrences)
    {
        // Insert the <docID,freq> pair inside the vector associated to the term
        term_to_inverted_index[term].push_back(std::make_pair(id, occurrences));
    }

    void write_to_disk(std::ostream& inverted_indices_teletype, std::ostream& lexicon_teletype);
};

}