# MIRCV's project: SEPP (Search Engine ++)

A C++ program that creates an inverted index structure from a set of text documents and a program that processes queries over such inverted index.

## Clone the Repository

To clone the repository, use the following command:

```bash
git clone --recursive https://github.com/scarburato/searchenginepp.git
```

## Dependencies

### Ubuntu

To ensure the program works on Ubuntu, make sure you have the following dependencies installed:

```bash
sudo apt-get update
sudo apt install build-essential cmake libstemmer-dev libhyperscan-dev
```

If you're not building on an amd64 (x86) system you may have to install `vectorscan` in place of `hyperscan`, on Ubuntu
the package to install should be named `libvectorscan-dev`.

### MacOS

Assuming you have already installed a C++ compiler and the (brew packet manager)[https://formulae.brew.sh/]; to compile
on MacOS, you need to install the following dependencies:

```shell
brew install cmake snowball hyperscan
```

If you're not building on an amd64 (x86) system, you have to install `vectorscan` in place of `hyperscan`.

The `CMakeLists.txt` file assumes that brew install its stuff in `/usr/local`, if your version of brew differs you have to
modify the `CMakeLists.txt` file accordingly.

## Build

Navigate to the project directory and use the following commands:

```bash
mkdir build
cd build/
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_STEMMER:BOOL=ON -DFIX_MSMARCO_LATIN1:BOOL=ON -DTEXT_FULL_LATIN1_CASE:BOOL=ON ..
make -j8
```

you should replace the `-j8` flag with the number of cores in your CPU

### CMake options

Those are the building options:

- `USE_STEMMER` used to enable or disable Snowball's stemmer and stopword removal
- `FIX_MSMARCO_LATIN1` used to enable or disable heuristic and encoding fix for certain wronly encoded docs in MSMARCO. If you are not using MSMARCO you should put this flag to OFF (default option is OFF)
- `TEXT_FULL_LATIN1_CASE` replaces the ASCII-only lower-case algorithm with a latin1 (a larger subset of utf8) lower-case
- `USE_FAST_LOG` replaces the floating point version of log with a faster integer version. It doesn't improve performance by much

### Run tests

Go in the `build/` directory and use the following command:

```bash
tests/Google_Tests_run
```

If all tests return RUN OK, it means all tests passed successfully.

## Build the Index

To read the collection efficiently, use the following command:

```bash
time tar -xOzf ../data/collection.tar.gz collection.tsv | ./builder
```

The use of `tar` alongside UNIX's pipes, allows the system to decompress the collection
in blocks, and keep in the input buffer of only the chunk that's being proccessed at the moment,
thus removing the necessity to decompress the file separately and to load it in memory all at once.

## Run the Query Processor

To run the query processor, run the following command:

```bash
./engine [options] [data]
```

where `[options]` are:

- `-b|--batch` to run the query processor in batch mode, that is to read queries in the format `query_id\tquery_text` 
   from stdin, if omitted the programm will accept only the query text from stdin
- `-k|--top-k` to specify the number of top documents to return for each query (default is 10)
- `-t|--threads` to specify the number of threads to use (default is 1). It is not advisable to use more than one if
   all chunks are on the same disk
- `-s|--score` to specify the scoring function to use for the query processor.
  The available algorithms are:
   - `bm25` to use the BM25 scoring function (default)
   - `tfidf` to use the TF-IDF scoring function
- `-a|--algorithm` to specify the algorithm to use for the query processor. The available algorithms are:
   - `daat|daat-disjunctive` to use the daat in disjunctive mode (default)
   - `daat-c|daat-conjunctive` to use the daat in conjunctive mode
   - `bmm` to use the BMM dynamic programming algorithm
- `-r|--run-name` to specify the name of the run (default is `MIRCV0`)

and `[data]` is the path to the data directory that contains the files (default is `data/`)

For example, you can run the query processor in batch mode with the BM25 scoring function and the DAAT algorithm to
produce a run file to use with trec_eval with the following command:

```bash
time ./engine -b -k20 -r MIRCV-DAAT-BM25-20  < ../../msmarco-test2020-queries.tsv > mircv-daat-bm25-20.run
```

## Additional Notes




