#pragma once

#include "Event.h"

#include <vector>
#include <mutex>
#include <memory>
#include <functional>

template<typename T>
using Ref = std::shared_ptr<T>;

class EventQueue
{
public:
	//=========================
	// Constructor/Destructor
	//=========================
	EventQueue() = default;
	~EventQueue() = default;

public:
	//=========================
	// Lifecycle Functions
	//=========================
	void Init(EventCallbackFn processQueueFunc);
public:
	//=========================
	// Modify Queue
	//=========================
	void SubmitEvent(Ref<Event> event);
	void ClearQueue();

	//=========================
	// Submit Queue
	//=========================
	void ProcessQueue();
private:
	//=========================
	// Internal Fields
	//=========================
	std::vector<Ref<Event>> m_EventQueue{};
	std::mutex m_EventQueueMutex{};
	EventCallbackFn m_ProcessQueueFunc{ nullptr };
};