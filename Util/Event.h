#pragma once

#include <string>
#include <functional>

//============================================================
// Events Namespace
//============================================================
// This namespace provides the main classes that allow the Event Pipeline
//		to work correctly. The intended use of classes in this namespace
//		is:
//		1. Create an OnEvent(Event) function in the main application layer that
//		links to other OnEvent(Event) functions depending on the particular scenerio.
//		(These linkages constitute the event pipeline)
//		2. Provide this main OnEvent(Event) function to an event generating section
//		of the engine such as the Windowing Library (GLFW currently) that can receive
//		input events such as KeyPressed events.
//		3. Add Dispatchers throughout the OnEvent(Event) function pipeline that link
//		to local functions that handle events. The dispatchers check if the event type
//		of the currently running event matches the dispatcher's type.
//		4. Implement logic to handle events in different scenerios using the m_Handled
//		member of the event class.

//==============================
// Event Type Enum
//==============================
// This enum represents all of the different event types present in
//		the application that are mutually exclusive. This list will
//		expand as more event types are required.
enum class EventType
{
	None = 0,
	// Application
	AppUpdate,
	KeyPressed
};

//==============================
// Event Category Enum
//==============================
// This enum provides an easy method to determine if a particular event
//		falls into one or more non-mutually exclusive event categories.
//		The enum is structured as a bitfield to allow a particular event
//		to have multiple categories. Ex: A MouseButtonEvent could be labeled
//		as a Mouse, MouseButton, and Input event.
enum EventCategory
{
	None = 1 << 0,
	Application = 1 << 1,
	Input = 1 << 2
	
};

//============================================================
// Event Class
//============================================================
// This Event class provides a uniform interface for different event
//		types to be routed through the same event pipeline. This is
//		an abstract class that holds multiple getter functions that
//		allow for easy identification of the event type among other
//		properties. This class also has a Handled member which
//		informs the event pipeline that the event has been used
//		to run a function call.
class Event
{
public:
	//==============================
	// Constructors and Destructors
	//==============================
	// This constructor is simply set to prevent object slicing
	//		during the destruction of the parent class.
	virtual ~Event() = default;

	//==============================
	// Getters/Setters
	//==============================
	// These getter functions provide a method to identify event types
	//		and manage the events externally. The ToString function is
	//		for debugging.
	virtual EventType GetEventType() const = 0;
	virtual int GetCategoryFlags() const = 0;
	// This function provides an easy method for determining if an event
	//		is part of a broader category since EventCategory's are bit
	//		fields.
	bool IsInCategory(EventCategory category)
	{
		return GetCategoryFlags() & category;
	}
};

using EventCallbackFn = std::function<void(Event*)>;

//============================================================
// Add Extra Update Event Class
//============================================================
class AppUpdateEvent : public Event
{
public:
	//==============================
	// Constructors and Destructors
	//==============================
	AppUpdateEvent(float deltaTime)
		: m_DeltaTime(deltaTime) {}

	//==============================
	// Getters/Setters
	//==============================

	float GetDeltaTime() const { return m_DeltaTime; }

	virtual EventType GetEventType() const override { return EventType::AppUpdate; }
	virtual int GetCategoryFlags() const override { return EventCategory::Application; }
private:
	float m_DeltaTime;
};

class KeyPressedEvent : public Event
{
public:
	//==============================
	// Constructors and Destructors
	//==============================
	// This constructor initializes both the m_KeyCode from the parent class
	//		and the m_IsRepeat member.
	KeyPressedEvent(const char keycode, bool isRepeat = false)
		: m_KeyCode(keycode), m_IsRepeat(isRepeat) {}

	//==============================
	// Getters/Setters
	//==============================
	bool IsRepeat() const { return m_IsRepeat; }
	char GetKeyCode() const { return m_KeyCode; }

	virtual EventType GetEventType() const override { return EventType::KeyPressed; }
	virtual int GetCategoryFlags() const override { return EventCategory::Input; }
private:
	// m_IsRepeat represents whether the Key has been held down long enough
	//		for the windowing system to label the key as repeating.
	bool m_IsRepeat;
	char m_KeyCode;
};