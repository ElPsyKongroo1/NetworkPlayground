#pragma once
#include "chrono"

class PassiveLoopTimer
{
public:
	//==============================
	// Constructors/Destructors
	//==============================
	PassiveLoopTimer() = default;
	~PassiveLoopTimer() = default;

	//==============================
	// Lifecycle Functions
	//==============================
	void InitializeTimer(std::chrono::nanoseconds updateDelta);
	void InitializeTimer(float updateDeltaSeconds);
	bool CheckForUpdate(std::chrono::nanoseconds timestep);

	//==============================
	// Getters/Setters
	//==============================
	// Manage constant frame time
	void SetUpdateDelta(std::chrono::nanoseconds newFrameTime);
	void SetUpdateDeltaFloat(float newFrameTimeSeconds);
private:
	//==============================
	// Internal Fields
	//==============================
	// Accumulation data
	std::chrono::nanoseconds m_Accumulator;

	// Configuration data
	std::chrono::nanoseconds m_UpdateDelta{ 1'000 * 1'000 * 1'000 / 60 };
};