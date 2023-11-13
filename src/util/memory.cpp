#include "memory.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * This procedure mmaps a file in memory
 * @param filename
 * @return
 */
static std::pair<void*, size_t> mmap_helper(char const *const filename)
{
	auto fd = open(filename, O_RDONLY);

	// get file's size
	struct stat st{};
	auto ret_stat= stat(filename, &st);

	if(ret_stat == -1)
		abort();

	// map file in memory
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

memory_mmap::~memory_mmap()
{
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
