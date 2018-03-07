// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modified version of Recast/Detour's source file

//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef DETOUROBSTACLEAVOIDANCE_H
#define DETOUROBSTACLEAVOIDANCE_H

#include "CoreMinimal.h"

struct dtObstacleCircle
{
	float p[3];				///< Position of the obstacle
	float vel[3];			///< Velocity of the obstacle
	float dvel[3];			///< Velocity of the obstacle
	float rad;				///< Radius of the obstacle
	float dp[3], np[3];		///< Use for side selection during sampling.
};

struct dtObstacleSegment
{
	float p[3], q[3];		///< End points of the obstacle segment
	unsigned char touch : 1;
	unsigned char canIgnore : 1;
};

class NAVMESH_API dtObstacleAvoidanceDebugData
{
public:
	dtObstacleAvoidanceDebugData();
	~dtObstacleAvoidanceDebugData();
	
	bool init(const int maxSamples);
	void reset();
	void addSample(const float* vel, const float ssize, const float pen,
				   const float vpen, const float vcpen, const float spen, const float tpen);
	
	void normalizeSamples();
	
	inline int getSampleCount() const { return m_nsamples; }
	inline const float* getSampleVelocity(const int i) const { return &m_vel[i*3]; }
	inline float getSampleSize(const int i) const { return m_ssize[i]; }
	inline float getSamplePenalty(const int i) const { return m_pen[i]; }
	inline float getSampleDesiredVelocityPenalty(const int i) const { return m_vpen[i]; }
	inline float getSampleCurrentVelocityPenalty(const int i) const { return m_vcpen[i]; }
	inline float getSamplePreferredSidePenalty(const int i) const { return m_spen[i]; }
	inline float getSampleCollisionTimePenalty(const int i) const { return m_tpen[i]; }

private:
	int m_nsamples;
	int m_maxSamples;
	float* m_vel;
	float* m_ssize;
	float* m_pen;
	float* m_vpen;
	float* m_vcpen;
	float* m_spen;
	float* m_tpen;
};

NAVMESH_API dtObstacleAvoidanceDebugData* dtAllocObstacleAvoidanceDebugData();
NAVMESH_API void dtFreeObstacleAvoidanceDebugData(dtObstacleAvoidanceDebugData* ptr);


static const int DT_MAX_PATTERN_DIVS = 32;		///< Max numver of adaptive divs.
static const int DT_MAX_PATTERN_RINGS = 4;		///< Max number of adaptive rings.
static const int DT_MAX_CUSTOM_SAMPLES = 16;	///< Max number of custom samples in single pattern

struct NAVMESH_API dtObstacleAvoidanceParams
{
	float velBias;
	float weightDesVel;
	float weightCurVel;
	float weightSide;
	float weightToi;
	float horizTime;
	unsigned char patternIdx;	///< [UE4] index of custom sampling pattern or 0xff for adaptive
	unsigned char adaptiveDivs;	///< adaptive
	unsigned char adaptiveRings;	///< adaptive
	unsigned char adaptiveDepth;	///< adaptive
};

// [UE4] custom sampling patterns
struct dtObstacleAvoidancePattern
{
	float angles[DT_MAX_CUSTOM_SAMPLES];	///< sample's angle (radians) from desired velocity direction
	float radii[DT_MAX_CUSTOM_SAMPLES];		///< sample's radius (0...1)
	int nsamples;							///< Number of samples
};

class NAVMESH_API dtObstacleAvoidanceQuery
{
public:
	dtObstacleAvoidanceQuery();
	~dtObstacleAvoidanceQuery();
	
	bool init(const int maxCircles, const int maxSegments, const int maxCustomPatterns);
	
	void reset();

	void addCircle(const float* pos, const float rad,
				   const float* vel, const float* dvel);
				   
	void addSegment(const float* p, const float* q, int flags = 0);

	// [UE4] store new sampling pattern
	bool setCustomSamplingPattern(int idx, const float* angles, const float* radii, int nsamples);

	// [UE4] get custom sampling pattern
	bool getCustomSamplingPattern(int idx, float* angles, float* radii, int* nsamples);

	// [UE4] sample velocity using custom patterns
	int sampleVelocityCustom(const float* pos, const float rad,
					 		 const float vmax, const float vmult,
							 const float* vel, const float* dvel, float* nvel,
							 const dtObstacleAvoidanceParams* params,
							 dtObstacleAvoidanceDebugData* debug = 0);

	int sampleVelocityAdaptive(const float* pos, const float rad,
							   const float vmax, const float vmult,
							   const float* vel, const float* dvel, float* nvel,
							   const dtObstacleAvoidanceParams* params, 
							   dtObstacleAvoidanceDebugData* debug = 0);
	
	// [UE4] main sampling function
	inline int sampleVelocity(const float* pos, const float rad,
		const float vmax, const float vmult,
		const float* vel, const float* dvel, float* nvel,
		const dtObstacleAvoidanceParams* params,
		dtObstacleAvoidanceDebugData* debug = 0)
	{
		return (params->patternIdx == 0xff) ?
			sampleVelocityAdaptive(pos, rad, vmax, vmult, vel, dvel, nvel, params, debug) :
			sampleVelocityCustom(pos, rad, vmax, vmult, vel, dvel, nvel, params, debug);
	}

	inline int getObstacleCircleCount() const { return m_ncircles; }
	const dtObstacleCircle* getObstacleCircle(const int i) { return &m_circles[i]; }

	inline int getObstacleSegmentCount() const { return m_nsegments; }
	const dtObstacleSegment* getObstacleSegment(const int i) { return &m_segments[i]; }

	// [UE4] sampling pattern count accessors
	inline int getCustomPatternCount() const { return m_maxPatterns; }

private:

	void prepare(const float* pos, const float* dvel);

	float processSample(const float* vcand, const float cs,
						const float* pos, const float rad,
						const float* vel, const float* dvel,
						dtObstacleAvoidanceDebugData* debug);

	dtObstacleCircle* insertCircle(const float dist);
	dtObstacleSegment* insertSegment(const float dist);

	dtObstacleAvoidanceParams m_params;
	float m_invHorizTime;
	float m_vmax;
	float m_invVmax;

	int m_maxPatterns;
	dtObstacleAvoidancePattern* m_customPatterns;

	int m_maxCircles;
	dtObstacleCircle* m_circles;
	int m_ncircles;

	int m_maxSegments;
	dtObstacleSegment* m_segments;
	int m_nsegments;
};

NAVMESH_API dtObstacleAvoidanceQuery* dtAllocObstacleAvoidanceQuery();
NAVMESH_API void dtFreeObstacleAvoidanceQuery(dtObstacleAvoidanceQuery* ptr);


#endif // DETOUROBSTACLEAVOIDANCE_H
