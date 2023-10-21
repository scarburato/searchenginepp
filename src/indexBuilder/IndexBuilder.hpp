#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../index/types.hpp"

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
    void add(std::string term, docid_t id, freq_t occurences)
    {
        // If the term is not in the index I create the vector
        if(term_to_inverted_index.find(term) == term_to_inverted_index.end())
        {
            term_to_inverted_index[term] = std::vector<std::pair<docid_t, freq_t>>();
        }

        // Insert the <docID,freq> pair inside the vector associated to the term
        term_to_inverted_index[term].push_back(std::make_pair(id, occurences));

    }

    void write_to_disk(std::ostream& inverted_indices_teletype, std::ostream& lexicon_teletype);
};

}