//
// Created by Tim on 11/11/2017.
//

#include "SpecificThreadPool.h"
#include "logger.h"

SpecificThreadPool::SpecificThreadPool(uint32_t numThreads)
{
	resize(numThreads);
}

void SpecificThreadPool::clear()
{
	for(auto& thread : threads)
		thread->setEnding(true);

	for(auto& thread : threads)
		thread->workerThread.join();
}

void SpecificThreadPool::resize(uint32_t num)
{
	clear();
	for(auto& thread : threads)
		delete thread;
	threads.clear();

	threads.reserve(num);
	for(int i = 0; i < num; i++)
	{
		threads.emplace_back(new SpecificThread(i));
	}
}

void SpecificThreadPool::wait()
{
	for(auto& thread : threads)
		thread->wait();
}

void SpecificThreadPool::addJob(std::function<void(void)> newJob, int threadIndex)
{
	if(threads.empty())
		throw std::runtime_error("No thread count allocated");
	if(threadIndex >= threads.size())
		throw std::runtime_error("Invalid thread index");

	threads[threadIndex]->addJob(newJob);
}

void SpecificThreadPool::destroy()
{
	clear();
}



SpecificThread::SpecificThread(int i)
{
	workerThread = std::thread(std::bind(threadEntry, this, i));
}

void SpecificThread::wait()
{
	std::unique_lock<std::mutex> lock(syncMutex);
	waitCondition.wait(lock, [this] { return jobs.empty() && !active; });
}

void SpecificThread::setEnding(bool newEnding)
{
	std::unique_lock<std::mutex> lock(syncMutex);
	ending = newEnding;
	jobCondition.notify_all();
}

void SpecificThread::threadEntry(int i)
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
			active = true;
		}

		job();

		{
			//Trigger threadpool wait check
			std::unique_lock<std::mutex> lock(syncMutex);
			active = false;
			waitCondition.notify_one();
		}
	}
}

void SpecificThread::addJob(std::function<void(void)> newJob)
{
	std::lock_guard<std::mutex> syncLockGuard(syncMutex);
	jobs.emplace(std::move(newJob));
	jobCondition.notify_one();
}
