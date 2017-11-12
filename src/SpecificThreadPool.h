//
// Created by Tim on 11/11/2017.
//

#ifndef VULKANITE_SPECIFICTHREADPOOL_H
#define VULKANITE_SPECIFICTHREADPOOL_H

#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

class SpecificThread
{
	std::mutex syncMutex;
	std::queue<std::function<void(void)> > jobs;
	std::condition_variable jobCondition;
	std::condition_variable waitCondition;
	bool active = false;

	void threadEntry(int i);

public:
	explicit SpecificThread(int i);
	void addJob(std::function<void(void)> newJob);
	void setEnding(bool newEnding);

	void wait();

	std::thread workerThread;
	bool ending = false;
};

class SpecificThreadPool
{
	std::vector<SpecificThread*> threads;
	void clear();

public:
	explicit SpecificThreadPool() = default;
	explicit SpecificThreadPool(uint32_t numThreads);

	void wait();
	void resize(uint32_t num);
	void destroy();

	void addJob(std::function<void(void)> newJob, int threadIndex);
};


#endif //VULKANITE_SPECIFICTHREADPOOL_H
