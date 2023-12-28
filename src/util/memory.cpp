#include "memory.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * This procedure mmaps a file in memory
 * @param filename
 * @return a pair containing the pointer to the mapped file and its size
 */
static std::pair<void*, size_t> mmap_helper(char const *const filename)
{
	auto fd = open(filename, O_RDONLY);

	// Get file's size
	struct stat st{};
	auto ret_stat= stat(filename, &st);

	if(ret_stat == -1)
		abort();

	// Map file in memory
	void* mapped = (uint8_t *)mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);

	return {mapped, st.st_size};
}

memory_mmap::memory_mmap(const std::string &filename)
{
	auto [b, s] = mmap_helper(filename.c_str());
	buff = static_cast<uint8_t *>(b);
	buff_size = s;
}

memory_mmap::memory_mmap(memory_mmap &&other)
{
	buff = other.buff;
	buff_size = other.buff_size;

	other.buff = nullptr;
	other.buff_size = 0;
}

memory_mmap::~memory_mmap()
{
	if(buff)
		munmap((void *) buff, buff_size);
}

std::pair<uint8_t *, size_t> memory_mmap::get() const
{
	return {buff, buff_size};
}

std::pair<uint8_t *, size_t> memory_buffer::get() const
{
	return {buff, buff_size};
}
