#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

class thread_pool
{
	using Task = std::function<void()>;

	// Workers
	std::vector<std::thread> workers;
	std::unique_ptr<std::atomic_bool[]> idling;
	std::atomic_bool halt = false;

	// and Resources
	std::queue<Task> jobs;

	// and mechanisms, too
	std::condition_variable jobs_queue_cv;
	std::condition_variable jobs_queue_popped_cv;
	std::mutex jobs_queue_mutex;

	std::condition_variable done_cv;
	std::mutex done_mutex;

	// workers' main loop
	void worker_procedure(size_t worker_id);

public:
	explicit thread_pool(size_t n_workers = std::thread::hardware_concurrency());
	~thread_pool();
	void wait_for_empty_queue();
	void wait_for_free_worker();
	void wait_all_jobs();
	void add_job(const Task&& job);
};
