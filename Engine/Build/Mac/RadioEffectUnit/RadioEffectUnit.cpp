/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit.cpp
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "AUEffectBase.h"
#include "RadioEffectUnitVersion.h"

#include <AudioUnit/AudioUnitProperties.h>

typedef unsigned int UBOOL;
typedef float FLOAT;
#define PI 					(3.1415926535897932)
inline FLOAT 	appCos( FLOAT Value ) 			{ return cosf(Value); }
inline FLOAT	appTan( FLOAT Value )			{ return tanf(Value); }
inline FLOAT	appPow( FLOAT A, FLOAT B )		{ return powf(A,B); }

/*-----------------------------------------------------------------------------
 FBandPassFilter
 -----------------------------------------------------------------------------*/
class FBandPassFilter
{
private:
	
	FLOAT Coefficient0;
	FLOAT Coefficient1;
	FLOAT Coefficient2;
	FLOAT Coefficient3;
	FLOAT Coefficient4;
	
	FLOAT Z0;
	FLOAT Z1;
	FLOAT Y0;
	FLOAT Y1;
	
	
	inline static FLOAT CalculateC( FLOAT BandwidthHz, FLOAT SampleRate )
	{
		const FLOAT Angle = PI * ( ( BandwidthHz * 0.5f ) / SampleRate );
		return appTan( Angle - 1.0f ) / appTan( 2.0f * Angle + 1.0f );
	}
	
	inline static FLOAT CalculateD( FLOAT CenterFrequencyHz, FLOAT SampleRate )
	{
		const FLOAT Angle = 2.0f * PI * CenterFrequencyHz / SampleRate;
		return -appCos( Angle );
	}
	
public:
	
	/**
	 * Constructor (default).
	 */
	FBandPassFilter( void )
	:	Coefficient0( 0.0f )
	,	Coefficient1( 0.0f )
	,	Coefficient2( 0.0f )
	,	Coefficient3( 0.0f )
	,	Coefficient4( 0.0f )
	,	Z0( 0.0f )
	,	Z1( 0.0f )
	,	Y0( 0.0f )
	,	Y1( 0.0f )
	{
	}
	
	inline void Initialize( FLOAT FrequencyHz, FLOAT BandwidthHz, FLOAT SampleRate )
	{
		const FLOAT C = CalculateC( BandwidthHz, SampleRate );
		const FLOAT D = CalculateD( FrequencyHz, SampleRate );
		
		const FLOAT A0 = 1.0f;
		const FLOAT A1 = D * ( 1.0f - C );
		const FLOAT A2 = -C;
		const FLOAT B0 = 1.0f + C;
		const FLOAT B1 = 0.0f;
		const FLOAT B2 = -B0;
		
		Coefficient0 = B0 / A0;
		
		Coefficient1 = +B1 / A0;
		Coefficient2 = +B2 / A0;
		Coefficient3 = -A1 / A0;
		Coefficient4 = -A2 / A0;
		
		Z0 = Z1 = Y0 = Y1 = 0.0f;
	}
	
	inline FLOAT Process( FLOAT Sample )
	{
		const FLOAT Y	= Coefficient0 * Sample
		+ Coefficient1 * Z0
		+ Coefficient2 * Z1
		+ Coefficient3 * Y0
		+ Coefficient4 * Y1;
		Z1 = Z0;
		Z0 = Sample;
		Y1 = Y0;
		Y0 = Y;
		
		return Y;
	}
};

/*-----------------------------------------------------------------------------
 Global utility classes for generating a radio distortion effect.
 -----------------------------------------------------------------------------*/
static FBandPassFilter      GFinalBandPassFilter;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____RadioEffectUnit

static UBOOL bGLocalized = FALSE;

class RadioEffectUnit : public AUEffectBase
{
public:
								RadioEffectUnit(AudioUnit Component);
	
	virtual AUKernelBase *		NewKernel() { return new RadioEffectKernel(this); }
	
    virtual	OSStatus			GetParameterValueStrings(	AudioUnitScope			InScope,
                                                            AudioUnitParameterID	InParameterID,
                                                            CFArrayRef *			OutStrings	);
    
	virtual	OSStatus			GetParameterInfo(	AudioUnitScope			InScope,
                                                    AudioUnitParameterID	InParameterID,
													AudioUnitParameterInfo	&OutParameterInfo	);
    
	virtual OSStatus			GetPropertyInfo(	AudioUnitPropertyID		InID,
													AudioUnitScope			InScope,
													AudioUnitElement		InElement,
													UInt32 &				OutDataSize,
													Boolean	&				OutWritable );

	virtual OSStatus			GetProperty(		AudioUnitPropertyID InID,
													AudioUnitScope 		InScope,
													AudioUnitElement 	InElement,
													void *				OutData );
    
    // Some hosting apps will REQUIRE that you support this property (and others won't), but
    // it is advisable for maximal compatibility that you do support this (and report a conservative
    // but reasonable value.)
	virtual	bool				SupportsTail () { return true; }

		/*! @method Version */
	virtual OSStatus		Version() { return kRadioEffectUnitVersion; }


protected:
	class RadioEffectKernel : public AUKernelBase		// most real work happens here
	{
	public:
		RadioEffectKernel(AUEffectBase *InAudioUnit )
			: AUKernelBase(InAudioUnit)
		{
// Initialize per-channel state of this effect processor
		}

// Required overides for the process method for this effect
		// processes one channel of interleaved samples
		virtual void 		Process(	const AudioUnitSampleType *	InSourceP,
										AudioUnitSampleType *		InDestP,
										UInt32						InFramesToProcess,
										UInt32						InNumChannels,
                                        bool &						IOSilence);

        virtual void		Reset();

	//private: //state variables...
	};
    
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

AUDIOCOMPONENT_ENTRY(AUBaseFactory, RadioEffectUnit)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Unique constants for this effect

// parameters
static const FLOAT kDefaultValue_ChebyshevPowerMultiplier = 2.0f;
static const FLOAT kDefaultValue_ChebyshevPower = 5.0f;
static const FLOAT kDefaultValue_ChebyshevMultiplier = 3.0f;
static const FLOAT kDefaultValue_ChebyshevCubedMultiplier = 5.0f;

static CFStringRef kChebyshevPowerMultiplierName = CFSTR("Chebyshev Power Multiplier");
static CFStringRef kChebyshevPowerName = CFSTR("Chebyshev Power");
static CFStringRef kChebyshevMultiplierName = CFSTR("Chebyshev Multiplier");
static CFStringRef kChebyshevCubedMultiplierName = CFSTR("Chebyshev Cubed Multiplier");

enum {
	RadioParam_ChebyshevPowerMultiplier,
	RadioParam_ChebyshevPower,
    RadioParam_ChebyshevMultiplier,
	RadioParam_ChebyshevCubedMultiplier,
	RadioNumberOfParameters
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::RadioEffectUnit
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RadioEffectUnit::RadioEffectUnit(AudioUnit Component)
	: AUEffectBase(Component)
{
	CreateElements();

	if (!bGLocalized) {		
		// Because we are in a component, we need to load our bundle by identifier so we can access our localized strings
		// It is important that the string passed here exactly matches that in the Info.plist Identifier string
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR("com.epicgames.audiounit.radio") );
		
		if (bundle != NULL) {
			kChebyshevPowerMultiplierName = CFCopyLocalizedStringFromTableInBundle(kChebyshevPowerMultiplierName, CFSTR("Localizable"), bundle, CFSTR(""));
			kChebyshevPowerName = CFCopyLocalizedStringFromTableInBundle(kChebyshevPowerName, CFSTR("Localizable"), bundle, CFSTR(""));
			kChebyshevMultiplierName = CFCopyLocalizedStringFromTableInBundle(kChebyshevMultiplierName, CFSTR("Localizable"), bundle, CFSTR(""));	
			kChebyshevCubedMultiplierName = CFCopyLocalizedStringFromTableInBundle(kChebyshevCubedMultiplierName, CFSTR("Localizable"), bundle, CFSTR(""));	
		}
		bGLocalized = TRUE; //so never pass the test again...
	}

	GFinalBandPassFilter.Initialize( 2000.0f, 400.0f, GetSampleRate() );

	SetParameter(RadioParam_ChebyshevPowerMultiplier, 	kDefaultValue_ChebyshevPowerMultiplier );
	SetParameter(RadioParam_ChebyshevPower, 			kDefaultValue_ChebyshevPower );
	SetParameter(RadioParam_ChebyshevMultiplier, 		kDefaultValue_ChebyshevMultiplier );
	SetParameter(RadioParam_ChebyshevCubedMultiplier,	kDefaultValue_ChebyshevCubedMultiplier );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::GetParameterValueStrings
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			RadioEffectUnit::GetParameterValueStrings(	AudioUnitScope			InScope,
                                                                AudioUnitParameterID	InParameterID,
                                                                CFArrayRef *			OutStrings)
{
    return kAudioUnitErr_InvalidProperty;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::GetParameterInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			RadioEffectUnit::GetParameterInfo(	AudioUnitScope			InScope,
                                                        AudioUnitParameterID	InParameterID,
                                                        AudioUnitParameterInfo	&OutParameterInfo )
{
	OSStatus Result = noErr;

	OutParameterInfo.flags = 	kAudioUnitParameterFlag_IsWritable
						|		kAudioUnitParameterFlag_IsReadable;
    
    if (InScope == kAudioUnitScope_Global) {
        switch(InParameterID)
        {
            case RadioParam_ChebyshevPowerMultiplier:
                AUBase::FillInParameterName (OutParameterInfo, kChebyshevPowerMultiplierName, false);
                OutParameterInfo.unit = kAudioUnitParameterUnit_Ratio;
                OutParameterInfo.minValue = 0.0;
                OutParameterInfo.maxValue = 100.0;
                OutParameterInfo.defaultValue = kDefaultValue_ChebyshevPowerMultiplier;
                break;
    
            case RadioParam_ChebyshevPower:
                AUBase::FillInParameterName (OutParameterInfo, kChebyshevPowerName, false);
                OutParameterInfo.unit = kAudioUnitParameterUnit_Ratio;
                OutParameterInfo.minValue = 0.0;
                OutParameterInfo.maxValue = 100.0;
                OutParameterInfo.defaultValue = kDefaultValue_ChebyshevPower;
                break;
            
            case RadioParam_ChebyshevMultiplier:
                AUBase::FillInParameterName (OutParameterInfo, kChebyshevMultiplierName, false);
                OutParameterInfo.unit = kAudioUnitParameterUnit_Ratio;
                OutParameterInfo.minValue = 0.0;
                OutParameterInfo.maxValue = 100.0;
                OutParameterInfo.defaultValue = kDefaultValue_ChebyshevMultiplier;
                break;
 
			case RadioParam_ChebyshevCubedMultiplier:
                AUBase::FillInParameterName (OutParameterInfo, kChebyshevCubedMultiplierName, false);
                OutParameterInfo.unit = kAudioUnitParameterUnit_Ratio;
                OutParameterInfo.minValue = 0.0;
                OutParameterInfo.maxValue = 100.0;
                OutParameterInfo.defaultValue = kDefaultValue_ChebyshevCubedMultiplier;
                break;
           
            default:
                Result = kAudioUnitErr_InvalidParameter;
                break;
        }
	} else {
        Result = kAudioUnitErr_InvalidParameter;
    }
    
	return Result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::GetPropertyInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			RadioEffectUnit::GetPropertyInfo (AudioUnitPropertyID		InID,
												AudioUnitScope					InScope,
												AudioUnitElement				InElement,
												UInt32 &						OutDataSize,
												Boolean &						OutWritable)
{
	if (InScope == kAudioUnitScope_Global) {
		switch (InID) {
			case kAudioUnitProperty_ParameterStringFromValue:
				OutWritable = false;
				OutDataSize = sizeof (AudioUnitParameterStringFromValue);
				return noErr;
            
			case kAudioUnitProperty_ParameterValueFromString:
				OutWritable = false;
				OutDataSize = sizeof (AudioUnitParameterValueFromString);
				return noErr;
		}
	}
	return AUEffectBase::GetPropertyInfo (InID, InScope, InElement, OutDataSize, OutWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::GetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			RadioEffectUnit::GetProperty (AudioUnitPropertyID 	InID,
									  AudioUnitScope 					InScope,
									  AudioUnitElement			 		InElement,
									  void *							OutData)
{
	if (InScope == kAudioUnitScope_Global) {
		switch (InID) {            
			case kAudioUnitProperty_ParameterValueFromString:
				return kAudioUnitErr_InvalidParameter;

			case kAudioUnitProperty_ParameterStringFromValue:
				return kAudioUnitErr_InvalidParameter;
		}
	}
	return AUEffectBase::GetProperty (InID, InScope, InElement, OutData);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____RadioEffectKernel


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::RadioEffectKernel::Reset()
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		RadioEffectUnit::RadioEffectKernel::Reset()
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	RadioEffectUnit::RadioEffectKernel::Process
//
//		pass-through unit
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RadioEffectUnit::RadioEffectKernel::Process(	const AudioUnitSampleType *		InSourceP,
                                                    AudioUnitSampleType *			InDestP,
                                                    UInt32							InFramesToProcess,
                                                    UInt32							InNumChannels,
                                                    bool &							IOSilence )
{
// we should be doing something with the silence flag if it is true
// like not doing any work because:
// (1) we would only be processing silence and
// (2) we don't have any latency or tail times to worry about here
//
// So, we don't reset this flag, because it is true on input and we're not doing anything
// to it so we want it to be true on output.


// BUT: your code probably will need to take into account tail processing (or latency) that 
// it has once its input becomes silent... then at some point in the future, your output
// will also be silent.
    
	
	UInt32 NumSampleFrames = InFramesToProcess;
	const AudioUnitSampleType *SourceP = InSourceP;
	AudioUnitSampleType *DestP = InDestP;

	const FLOAT ChebyshevPowerMultiplier = GetParameter(RadioParam_ChebyshevPowerMultiplier);
	const FLOAT ChebyshevPower = GetParameter(RadioParam_ChebyshevPower);
	const FLOAT ChebyshevCubedMultiplier = GetParameter(RadioParam_ChebyshevCubedMultiplier);
	const FLOAT ChebyshevMultiplier = GetParameter(RadioParam_ChebyshevMultiplier);

	while (NumSampleFrames-- > 0) {
		AudioUnitSampleType InputSample = *SourceP;
		SourceP += InNumChannels;	// advance to next frame (e.g. if stereo, we're advancing 2 samples);
									// we're only processing one of an arbitrary number of interleaved channels

			// here's where you do your DSP work

		// Early-out of processing if the sample is zero because a zero sample 
		// will still create some static even if no audio is playing.
		if( InputSample != 0.0f ) {
			// Waveshape it
			const FLOAT SampleCubed = InputSample * InputSample * InputSample;
			InputSample = ( ChebyshevPowerMultiplier * appPow( InputSample, ChebyshevPower ) ) - ( ChebyshevCubedMultiplier * SampleCubed ) + ( ChebyshevMultiplier * InputSample );
			
			// Again with the bandpass
			InputSample = GFinalBandPassFilter.Process( InputSample );
		}
		
		*DestP = InputSample;
		DestP += InNumChannels;
	}
}
