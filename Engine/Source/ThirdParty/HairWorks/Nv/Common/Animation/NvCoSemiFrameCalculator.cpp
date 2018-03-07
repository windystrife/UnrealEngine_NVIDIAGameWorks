#include "NvCoSemiFrameCalculator.h"

namespace nvidia { 
namespace Common {

SemiFrameCalculator::SemiFrameCalculator()
{
	setMaxTimeStep(1.0f / 30.0f);
}

Void SemiFrameCalculator::setMaxTimeStep(Float maxTimeStep)
{
	NV_CORE_ASSERT(maxTimeStep > 0.0f);	

	m_maxTimeStep = maxTimeStep;
	m_recipMaxTimeStep = 1.0f / maxTimeStep;
	m_remainingTime = m_maxTimeStep * (1.0f / 2.0f);
	m_timeStep = 0.0f;
}

Int SemiFrameCalculator::addTime(Float elapsedTime)
{
	if (elapsedTime <= 0.0f)
	{
		return 0;
	}

	const Float halfMaxTimeStep = m_maxTimeStep * (1.0f / 2.0f);

	if (m_remainingTime < halfMaxTimeStep)
	{
		// There is remaining time.
		m_remainingTime += elapsedTime;
		if (m_remainingTime < halfMaxTimeStep)
		{
			// We still haven't completed the minimum time step
			return 0;
		}
		// Fix the elapsed time
		elapsedTime = m_remainingTime - halfMaxTimeStep;
		// Remaining time is disabled
		m_remainingTime = halfMaxTimeStep;
	}

	if (elapsedTime <= halfMaxTimeStep)
	{
		// We need to do a single step and interp
		m_remainingTime = elapsedTime;
		// Set the timestep for a simulation step 'into the future'
		m_timeStep = halfMaxTimeStep;	
		return 1;
	}

	// Calculate the amount of full timesteps needed
	const Float numMaxTimeSteps = elapsedTime * m_recipMaxTimeStep;
	if (numMaxTimeSteps < 1.0f)
	{
		// Must be between halfMaxTimeStep and maxTimeStep
		m_timeStep = elapsedTime;
		return 1;
	}
	else
	{
		// Calculate the number of steps need, taking into account if there is a fraction left over
		Int numSimSteps = Int(numMaxTimeSteps);
		numSimSteps += Int(numMaxTimeSteps > numSimSteps); 
		m_timeStep = elapsedTime / numSimSteps;
		return numSimSteps;
	}
}

} // namespace Common
} // namespace nvidia
