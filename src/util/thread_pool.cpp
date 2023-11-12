#include <cassert>
#include "thread_pool.hpp"

thread_pool::thread_pool(size_t n_workers):
	idling(new std::atomic_bool[n_workers])
{
	for(size_t i = 0; i < n_workers; ++i)
	{
		workers.emplace_back(&thread_pool::worker_procedure, this, i);
		idling[i] = true;
	}
}

thread_pool::~thread_pool()
{
	wait_all_jobs();

	// shutdown signal
	halt = true;
	{ // Critical section, wake up workers
		std::lock_guard queue_lock(jobs_queue_mutex);
		jobs_queue_cv.notify_all();
	}

	for(auto& worker : workers)
		worker.join();
}

void thread_pool::add_job(const Task&& job)
{
	std::lock_guard queue_lock{jobs_queue_mutex};
	jobs.emplace(job); // push emplace
	jobs_queue_cv.notify_one();
}

template<class T>
static T front_and_pop(std::queue<T>& q)
{
	auto t = q.front();
	q.pop();
	return t;
}

void thread_pool::worker_procedure(size_t worker_id)
{
	while(true)
	{
		// Critical section: wait for a job
		auto [stop, job] = [&] () -> std::pair<bool, Task>
		{
			std::unique_lock queue_lock(jobs_queue_mutex);
			jobs_queue_cv.wait(queue_lock, [&]{ return halt or not jobs.empty(); });

			// HALT
			if (halt and jobs.empty())
				return {true, nullptr};

			assert(not jobs.empty());

			// Found a job
			idling[worker_id] = false;
			jobs_queue_popped_cv.notify_all();
			return {false, front_and_pop(jobs)};
		}();

		if(not stop)
			job(); // Execute my job

		idling[worker_id] = true;

		// Signal task completion
		std::lock_guard queue_lock(done_mutex);
		done_cv.notify_all();

		if(stop)
			return;
	}
}

void thread_pool::wait_all_jobs()
{
	{// Critical section, wait the queue to be empty
		std::unique_lock queue_lock(jobs_queue_mutex);
		jobs_queue_popped_cv.wait(queue_lock, [&]{ return jobs.empty(); });
	}

	// Now wait for all threads to be idle
	for(size_t worker_id = 0; worker_id < workers.size(); ++worker_id)
	{
		if(idling[worker_id])
			continue;

		std::unique_lock done_lock(done_mutex);
		done_cv.wait(done_lock, [&] () -> bool {return idling[worker_id];});
	}
}
