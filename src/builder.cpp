#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include "index/types.hpp"

using namespace index;

const int CHUNK_SIZE = 3; // number of documents to read
/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */
void process_chunk(std::unique_ptr<std::vector<std::string>> chunk, docid_t base_id) {
    for (const auto &line : *chunk) {
        std::istringstream iss(line);
        std::string pid, text;
        if (std::getline(iss, pid, '\t') && std::getline(iss, text, '\n')) {
            docno_t docno = std::stoul(pid);
            // esempio stampa docno e text
            std::cout << "docno: " << docno << ", text: " << text << std::endl;
        }
    }
}


int main()
{
    const int CHUNK_SIZE = 10; // example for chunk size
    std::ifstream file("collection.tsv");
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }

    std::string line;
    docid_t base_id = 0; // Initialize base_id
    int line_count = 0;
    std::unique_ptr<std::vector<std::string>> chunk(new std::vector<std::string>());

    while (std::getline(file, line)) {
        chunk->push_back(line);
        line_count++;

        if (line_count == CHUNK_SIZE) {
            process_chunk(std::move(chunk), base_id);
            base_id += line_count;
            line_count = 0;
            chunk.reset(new std::vector<std::string>());
        }
    }

    // Process the remaining lines which are less than CHUNK_SIZE
    if (!chunk->empty()) {
        process_chunk(std::move(chunk), base_id);
    }

    file.close();
    return 0;
}