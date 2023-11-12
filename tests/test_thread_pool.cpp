#include <algorithm>
#include <random>
#include "gtest/gtest.h"
#include "util/thread_pool.hpp"

TEST(ThreadPool, test0)
{
	std::vector tests = {1, 2, 8, 8, 16, 16, 16, 32};
	std::array<int, 6'300> numbers{};
	int n = 1;
	std::ranges::generate(numbers.begin(), numbers.end(), [&n]() mutable { return n++; });

	for(auto n_workers : tests)
	{
		std::mt19937 gen(0xcafebabe + n_workers); // mersenne_twister_engine seeded with rd()
		std::uniform_int_distribution<> distrib(0, 300);

		std::mutex result_mutex;
		std::vector<int> result;
		result.reserve(numbers.size());

		{
			thread_pool pool(n_workers);

			for (auto number: numbers)
			{
				auto sleep_for = distrib(gen);
				pool.add_job([&, number, sleep_for] {
					std::this_thread::sleep_for(std::chrono::microseconds(sleep_for));
					std::lock_guard guard(result_mutex);
					result.push_back(number);
				});
			}

			pool.wait_all_jobs();
		}

		ASSERT_EQ(numbers.size(), result.size());
		std::sort(result.begin(), result.end());
		for (size_t i = 0; i < numbers.size(); i++)
			ASSERT_EQ(numbers[i], result[i]) << " at index " << i << std::endl;
	}
}

TEST(ThreadPool, test_destructor)
{
	constexpr size_t cycles = 100;
	std::atomic_int n = 0;

	{ // pool destructor test
		thread_pool pool(1);
		for(size_t i = 0; i < cycles; i++)
			pool.add_job([&n] {n += 1;});
	}

	ASSERT_EQ(n, cycles);
}
