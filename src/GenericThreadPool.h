//
// Created by Tim on 11/11/2017.
//

#ifndef VULKANITE_THREADPOOL_H
#define VULKANITE_THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

class GenericThreadPool
{
	std::vector<std::thread> threads;
	std::queue<std::function<void(void)> > jobs;
	std::mutex syncMutex;
	//Used by each thread to wait for new jobs
	std::condition_variable jobCondition;
	//Used by thread pool to wait for no jobs, notified by each thread
	std::condition_variable waitCondition;
	bool ending = false;
	int activeCounter = 0;

	void threadEntry(int i);
	void clear();

public:
	explicit GenericThreadPool() = default;
	explicit GenericThreadPool(int numThreads);
	void addJob(std::function<void(void)> newJob);

	void wait();
	void resize(int num);
	void destroy();
};

#endif //VULKANITE_THREADPOOL_H
