# MIRCV's project: SEPP (Search Engine ++)

A C++ program that creates an inverted index structure from a set of text documents and a program that processes queries over such inverted index.

## Clone the Repository

To clone the repository, use the following command:

```bash
git clone https://github.com/scarburato/searchenginepp.git
```

## Dependencies

To ensure the program works on Ubuntu, make sure you have the following dependencies installed:

```bash
sudo apt-get update
sudo apt install build-essential cmake libstemmer-dev libhyperscan-dev libpcrecpp-dev
```

## Build

Navigate to the project directory and use the following commands:

```bash
mkdir build
cd build/
cmake -DCMAKE_BUILD_TYPE=Release -DFIX_MSMARCO_LATIN1=ON ..
make -j8
```

you should replace the `-j8` flag with the number of cores in your CPU

### CMake options

There are two CMAKE options:

- `USE_STEMMER` used to enable or disable Snowball's stemmer and stopword removal
- `FIX_MSMARCO_LATIN1` used to enable or disable heuristic and encoding fix for certain wronly encoded docs in MSMARCO. If you are not using MSMARCO you should put this flag to OFF (default option is OFF)

### Run tests

Go in build/ directory and use the following command:

```bash
tests/Google_Tests_run
```

If all tests return RUN ON, it means all tests passed successfully.

## Run

To read the collection efficiently, use the following command:

```bash
tar -xOzf ../data/collection.tar.gz collection.tsv | ./builder
```

The use of `tar` alongside UNIX's pipes, allows the system to decompress the collection
in blocks, and keep in the input buffer of only the chunk that's being proccessed at the moment,
thus removing the necessity to decompress the file separately and to load it in memory all at once.

## Additional Notes




