#ifndef NV_CO_FRAME_CALCULATOR_H
#define NV_CO_FRAME_CALCULATOR_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

namespace nvidia {
namespace Common {

/*! \brief Holds the current animation frame state, as potentially an interpolation between two frame indices, or 
if the interpolation is not used, as a single frame index. */
class FramePosition
{
	NV_CO_DECLARE_CLASS_BASE(FramePosition)
		/// Returns the index of a frame if the position can be interpreted as a single frame
		/// Ie that it doesn't require interpolation.
	NV_FORCE_INLINE IndexT getSingleIndex() const;
		/// Ctor with frame indices, and interpolation
	NV_FORCE_INLINE FramePosition(IndexT index, IndexT nextIndex, Float interp):
		m_index(index), m_nextIndex(nextIndex), m_interp(interp)
	{}
		/// Default Ctor sets to nothing
	NV_FORCE_INLINE FramePosition() {}

	Float m_interp;					///< Interpolation between frame index and next frame index. 0 is prev, 1 is current.
	IndexT m_index;					///< Frame index
	IndexT m_nextIndex;				///< Next frame index
};

/*! \brief A FrameCalculator can be used to calculate frame indices, and interpolation between frames of a fixed time frame based animation.

*/
class FrameCalculator
{
	NV_CO_DECLARE_CLASS_BASE(FrameCalculator)

		/// Add time (typically in seconds). Returns the number of full frames that have elapsed
	Int addTime(Float elapsedTime);

		/// Given a offset in time, returns the amount of frames need to be moved such that a valid FramePosition can be produced.
	Int calcRelativeFrameOffset(Float offset) const;
		/// Get the frame index taking into account an integral offset in frame
	Int calcFrameIndex(Int offset) const;

		// Gets current position
	FramePosition getPosition() const { return FramePosition(getFrameIndex(), getNextFrameIndex(), getInterp()); }
		/// Given an offset in time (generally in seconds) will return the position at that offset
	FramePosition calcRelativePosition(Float offset) const;

		/// The value between 0 and 1 that indicates position between current frame and next frame in an interpolation.
	NV_FORCE_INLINE Float getInterp() const { return m_remainingTime * m_recipTimeStep; }
		/// Get the index of the next frame
	NV_FORCE_INLINE Int getNextFrameIndex() const { return m_frame + 1; }
		/// Get the index of the current frame
	NV_FORCE_INLINE Int getFrameIndex() const { return m_frame; }

		/// Get the time remaining left over from last complete frame change. Always therefore >= 0 and < m_timeStep.
	NV_FORCE_INLINE Float getRemainingTime() const { return m_remainingTime; }

		/// Set the desired timeStep (the length of time to transition between frames), typically in seconds.
	Void setTimeStep(Float timeStep);
		/// Get the time step in seconds
	NV_FORCE_INLINE Float getTimeStep() const { return m_timeStep; }

		/// Set the total number of frames. Can be 0 or > 2. 0 means there is no frame constraint, otherwise will wrap frames.
	Void setNumFrames(Int numFrames);

		/// Initialize
	Void init(Int numFrames, Float timeStep);

		/// Default Ctor
	FrameCalculator();

protected:
	Float m_recipTimeStep;			///< The reciprocal of the time step
	Float m_timeStep;				///< Time between each frame
	Float m_remainingTime;			///< Time remaining between frames
	Int m_frame;					///< The current frame
	Int m_numFrames;				///< The number of frames (if 0 there is no frame constraint, otherwise aims to wrap)
};

// ---------------------------------------------------------------------------
NV_FORCE_INLINE IndexT FramePosition::getSingleIndex() const
{
	const Float eps = 1e-6f;
	if (m_index == m_nextIndex || m_interp < eps)
	{
		return m_index;
	}
	else if (m_interp >= 1.0f - eps)
	{
		return m_nextIndex;
	}
	// Cannot return as a single index
	return -1;
}


} // namespace Common 
} // namespace nvidia 

#endif // NV_CO_FRAME_CALCULATOR_H