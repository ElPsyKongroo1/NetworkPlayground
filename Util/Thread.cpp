#include "Thread.h"

void KGThread::StartThread(std::function<void()> workFunction)
{
	m_ThreadRunning = true;
	m_WorkFunction = workFunction;
	m_Thread = new std::thread(&KGThread::RunThread, this);
}

void KGThread::StopThread(bool withinThread)
{
	m_ThreadRunning = false;

	if (withinThread)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_BlockThreadMutex);
		m_ThreadBlocked = false;
		m_BlockThreadCV.notify_one();
	}

	m_Thread->join();
	delete m_Thread;
}

void KGThread::RunThread()
{
	while (m_ThreadRunning)
	{
		std::unique_lock<std::mutex> lock(m_BlockThreadMutex);

		// Handle blocking the thread if necessary
		if (m_ThreadBlocked)
		{
			m_BlockThreadCV.wait(lock);
		}
		
		m_WorkFunction();
	}
}

void KGThread::BlockThread()
{
	std::lock_guard<std::mutex> lock(m_BlockThreadMutex);
	m_ThreadBlocked = true;
}

void KGThread::ResumeThread()
{
	std::lock_guard<std::mutex> lock(m_BlockThreadMutex);
	m_ThreadBlocked = false;
	m_BlockThreadCV.notify_one();
}
