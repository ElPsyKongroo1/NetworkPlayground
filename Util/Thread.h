#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

class KGThread
{
public:
	void StartThread(std::function<void()> workFunction);
	void StopThread(bool withinThread = false);
	void RunThread();

	void BlockThread();
	void ResumeThread();

private:
	std::atomic<bool> m_ThreadRunning{ false };
	std::thread* m_Thread{ nullptr };
	std::mutex m_BlockThreadMutex{};
	std::condition_variable m_BlockThreadCV{};
	bool m_ThreadBlocked{ false };
	std::function<void()> m_WorkFunction{ nullptr };
};