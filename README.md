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

## Building and Running the Program

Navigate to the project directory and use the following commands:

```bash
mkdir build
cmake ..
make -j8
```

To read the collection efficiently, we use the following command:

```bash
tar -xOzf ../data/collection.tar.gz collection.tsv | ./builder
```

## Additional Notes




