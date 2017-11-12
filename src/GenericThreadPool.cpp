//
// Created by Tim on 11/11/2017.
//

#include "GenericThreadPool.h"
#include "logger.h"

GenericThreadPool::GenericThreadPool(int numThreads)
{
	resize(numThreads);
}

void GenericThreadPool::addJob(std::function<void(void)> newJob)
{
	if(threads.empty())
		throw std::runtime_error("No thread count allocated");

	std::lock_guard<std::mutex> syncLockGuard(syncMutex);

	jobs.emplace(std::move(newJob));
	jobCondition.notify_one();
}

void GenericThreadPool::threadEntry(int i)
{
	std::thread::id id = std::this_thread::get_id();
	Logger() << "Thread " << i << " #" << id;

	std::function<void(void)> job;
	while(1)
	{
		{
			std::unique_lock<std::mutex> lock(syncMutex);
			jobCondition.wait(lock, [this] { return ending || !jobs.empty(); });
			//Always execute remaining jobs in queue
			if(ending && jobs.empty())
				break;

			job = std::move(jobs.front());
			jobs.pop();
			//Prevent threadpool wait from continuing whilst still executing
			activeCounter++;
		}

		job();

		{
			//Trigger threadpool wait check
			std::unique_lock<std::mutex> lock(syncMutex);
			activeCounter--;
			waitCondition.notify_one();
		}
	}
}

void GenericThreadPool::wait()
{
	std::unique_lock<std::mutex> lock(syncMutex);
	waitCondition.wait(lock, [this] { return jobs.empty() && activeCounter == 0; });
}

void GenericThreadPool::clear()
{
	{
		std::unique_lock<std::mutex> lock(syncMutex);
		ending = true;
		jobCondition.notify_all();
	}

	for(auto& thread : threads)
		thread.join();
}

void GenericThreadPool::resize(int num)
{
	clear();
	threads.clear();
	ending = false;

	threads.reserve(static_cast<unsigned long long int>(num));
	for(int i = 0; i < num; i++)
	{
		threads.emplace_back(std::bind(threadEntry, this, i));
	}
}

void GenericThreadPool::destroy()
{
	clear();
}