#ifndef NV_CO_SEMI_FRAME_CALCULATOR_H
#define NV_CO_SEMI_FRAME_CALCULATOR_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

namespace nvidia {
namespace Common {

/*! \brief A SemiFrameCalculator can be used to calculate simulation steps for a Semi-fixed simulation. The minimum timestep is half the maximum timestep. */
class SemiFrameCalculator
{
	NV_CO_DECLARE_CLASS_BASE(SemiFrameCalculator)

		/// Add time (typically in seconds). Returns the number of simulation steps.
	Int addTime(Float elapsedTime);

		/// Get the time remaining left over from last complete frame change. Always therefore >= 0 and <= m_maxTimeStep / 2
	NV_FORCE_INLINE Float getRemainingTime() const { return m_remainingTime; }
		/// Get remaining time (with an integral offset of minimum timesteps)
	NV_FORCE_INLINE Float getRemainingTime(Int offset) const { return m_remainingTime + m_maxTimeStep * (1.0f / 2.0f) * offset; }

		/// Set the max desired timeStep (the length of time to transition between frames), typically in seconds.
		/// The calculator will return timesteps of sizes between timeStep / 2 and timeStep
	Void setMaxTimeStep(Float timeStep);
		/// Get the time step in seconds
	NV_FORCE_INLINE Float getTimeStep() const { return m_timeStep; }

		/// Returns >=1.0f if it is an interp, else returns the interpolation
	NV_FORCE_INLINE Float getInterp() const { return (m_remainingTime >= m_maxTimeStep * (1.0f / 2.0f)) ? 1.0f : (m_remainingTime * m_recipMaxTimeStep * (1.0f / 2.0f)); }

		/// Default Ctor
	SemiFrameCalculator();

protected:
	Float m_maxTimeStep;			///< The maximum timestep. The minimum timestep is half of this (assuming interping)
	Float m_recipMaxTimeStep;		///< The reciprocal maximum timestep
	Float m_timeStep;				///< The timestep for simulation
	Float m_remainingTime;			///< Time remaining between steps (if  m_maxTimeStep / 2 <= m_timeRemaining, means no remaining time)
};

} // namespace Common 
} // namespace nvidia 

#endif // NV_CO_SEMI_FRAME_CALCULATOR_H