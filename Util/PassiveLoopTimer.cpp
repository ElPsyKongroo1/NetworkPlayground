#include "PassiveLoopTimer.h"

void PassiveLoopTimer::InitializeTimer(std::chrono::nanoseconds updateDelta)
{
	using namespace std::chrono_literals;

	m_Accumulator = 0ns;
	m_UpdateDelta = updateDelta;
}

void PassiveLoopTimer::InitializeTimer(float updateDeltaSeconds)
{
	InitializeTimer(std::chrono::nanoseconds((long long)(updateDeltaSeconds * 1'000'000'000)));
}

bool PassiveLoopTimer::CheckForUpdate(std::chrono::nanoseconds timestep)
{
	m_Accumulator += timestep;

	if (m_Accumulator < m_UpdateDelta)
	{
		return false;
	}

	m_Accumulator -= m_UpdateDelta;
	return true;
}

void PassiveLoopTimer::SetUpdateDelta(std::chrono::nanoseconds newFrameTime)
{
	m_UpdateDelta = newFrameTime;
}

void PassiveLoopTimer::SetUpdateDeltaFloat(float newFrameTimeSeconds)
{
	m_UpdateDelta = std::chrono::nanoseconds((long long)(newFrameTimeSeconds * 1'000'000'000));
}
