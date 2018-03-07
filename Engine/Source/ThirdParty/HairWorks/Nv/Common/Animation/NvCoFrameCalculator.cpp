#include "NvCoFrameCalculator.h"

namespace nvidia { 
namespace Common {

FrameCalculator::FrameCalculator() :
	m_remainingTime(0.0f),
	m_frame(0),
	m_numFrames(0)
{
	setTimeStep(1.0f / 30.0f);
}

Void FrameCalculator::init(Int numFrames, Float timeStep)
{
	setNumFrames(numFrames);
	setTimeStep(timeStep);
}

Int FrameCalculator::addTime(Float elapsedTime)
{
	m_remainingTime += elapsedTime;
	if (m_remainingTime >= m_timeStep)
	{
		Int numFrames = Int(m_remainingTime * m_recipTimeStep);
		m_frame += numFrames;
		m_remainingTime -= numFrames * m_timeStep;
		if (m_numFrames > 0 && m_frame >= m_numFrames - 1)
		{
			m_frame = m_frame % (m_numFrames - 1);
		}
		return numFrames;
	}
	return 0;
}

Void FrameCalculator::setNumFrames(Int numFrames)
{
	NV_CORE_ASSERT(numFrames == 0 || numFrames >= 2);
	if (numFrames > 0)
	{
		m_frame = m_frame % numFrames;
		m_numFrames = numFrames;
	}
	else
	{
		m_frame = 0;
		m_numFrames = 0;
	}
}

FramePosition FrameCalculator::calcRelativePosition(Float offset) const
{
	if (m_numFrames == 1)
	{
		return FramePosition(0, 0, 0.0f);
	}

	const Int frameIndexOffset = calcRelativeFrameOffset(offset);
	const Float timeRemaining = m_remainingTime + offset - m_timeStep * frameIndexOffset;
	
	// Calculate the interp in range 0 to 1
	Float interp = timeRemaining * m_recipTimeStep;

	const Float eps = 1e-5f;
	NV_CORE_ASSERT(interp >= -eps && interp <= 1.0f + eps);

	interp = interp < 0.0f ? 0.0f : interp;
	interp = interp > 1.0f ? 1.0f : interp;

	const Int frameIndex = calcFrameIndex(frameIndexOffset);
	return FramePosition(frameIndex, frameIndex + 1, interp);
}

Int FrameCalculator::calcFrameIndex(Int offset) const
{
	Int frameIndex = m_frame + offset;
	
	if (m_numFrames == 0)
	{
		return frameIndex;
	}
	else if (m_numFrames == 1)
	{
		return 0;
	}

	const Int mod = m_numFrames - 1;
	if (frameIndex >= 0)
	{
		return (frameIndex >= mod) ? (frameIndex % mod) : frameIndex;
	}
	else
	{
		frameIndex = -frameIndex;
		frameIndex = (frameIndex >= mod) ? (frameIndex % mod) : frameIndex;
		return mod - frameIndex;
	}
}

Int FrameCalculator::calcRelativeFrameOffset(Float offset) const
{
	Float time = m_remainingTime + offset;
	if (time >= 0.0f && time < m_timeStep)
	{
		return 0;
	}
	return (time >= 0.0f) ? Int(time * m_recipTimeStep) : (-Int(-time * m_recipTimeStep) - 1);
}

Void FrameCalculator::setTimeStep(Float timeStep)
{
	NV_CORE_ASSERT(timeStep > 0.0f);

	m_timeStep = timeStep;
	m_recipTimeStep = 1.0f / timeStep;
}

} // namespace Common
} // namespace nvidia
