/********************************************************************************//**
\file      OVR_Audio.h
\brief     OVR Audio SDK public header file
\copyright 2015 Oculus VR, LLC All Rights reserved.
************************************************************************************/
#ifndef OVR_Audio_h
#define OVR_Audio_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Result type used by the OVRAudio API
#ifndef OVR_RESULT_DEFINED
#define OVR_RESULT_DEFINED
typedef int32_t ovrResult;
#endif

/// Success is zero, while all error types are non-zero values.
#ifndef OVR_SUCCESS_DEFINED
#define OVR_SUCCESS_DEFINED
#define ovrSuccess 0
#endif

/// Enumerates error codes that can be returned by OVRAudio
typedef enum 
{
   ovrError_AudioUnknown        = 2000,       ///< An unknown error has occurred.
   ovrError_AudioInvalidParam   = 2001,       ///< An invalid parameter, e.g. NULL pointer or out of range variable, was passed
   ovrError_AudioBadSampleRate  = 2002,       ///< An unsupported sample rate was declared
   ovrError_AudioMissingDLL     = 2003,       ///< The DLL or shared library could not be found
   ovrError_AudioBadAlignment   = 2004,       ///< Buffers did not meet 16b alignment requirements
   ovrError_AudioUninitialized  = 2005,       ///< audio function called before initialization
   ovrError_AudioHRTFInitFailure = 2006,      ///< HRTF provider initialization failed 
   ovrError_AudioBadVersion      = 2007,      ///< Mismatched versions between header and libs

   // Errors used by OVRSR
   ovrError_AudioSRBegin         = 2100,
   ovrError_AudioSREnd           = 2199,
} ovrAudioError;

#ifndef OVR_CAPI_h
typedef struct ovrPosef_ ovrPosef;
typedef struct ovrPoseStatef_ ovrPoseStatef;
#endif

#define OVR_AUDIO_MAJOR_VERSION 1
#define OVR_AUDIO_MINOR_VERSION 0
#define OVR_AUDIO_PATCH_VERSION 2

#ifdef _WIN32
#ifdef OVR_AUDIO_DLL
#define OVRA_EXPORT __declspec( dllexport )
#else
#define OVRA_EXPORT 
#endif
#define FUNC_NAME __FUNCTION__
#elif defined __APPLE__ 
#define OVRA_EXPORT 
#define FUNC_NAME 
#elif defined __linux__
#define OVRA_EXPORT __attribute__((visibility("default")))
#define FUNC_NAME 
#else
#error not implemented
#endif

/// Audio source flags
///
/// \see ovrAudio_SetAudioSourceFlags
typedef enum 
{
  ovrAudioSourceFlag_None                          = 0x0000,
                                                       
  ovrAudioSourceFlag_Spatialize2D_HINT_RESERVED    = 0x0001, ///< Perform lightweight spatialization only (not implemented)
  ovrAudioSourceFlag_Doppler_RESERVED              = 0x0002, ///< Apply Doppler effect (not implemented)
                                                       
  ovrAudioSourceFlag_WideBand_HINT                 = 0x0010, ///< Wide band signal (music, voice, noise, etc.)
  ovrAudioSourceFlag_NarrowBand_HINT               = 0x0020, ///< Narrow band signal (pure waveforms, e.g sine)
  ovrAudioSourceFlag_BassCompensation_DEPRECATED   = 0x0040, ///< Compensate for drop in bass from HRTF (deprecated)
  ovrAudioSourceFlag_DirectTimeOfArrival           = 0x0080, ///< Time of arrival delay for the direct signal

  ovrAudioSourceFlag_ReflectionsDisabled           = 0x0100, ///< Disable reflections and reverb for a single AudioSource

  ovrAudioSourceFlag_DisableResampling_RESERVED    = 0x8000, ///< Disable resampling IR to output rate, INTERNAL USE ONLY

} ovrAudioSourceFlag;

/// Audio source attenuation mode
///
/// \see ovrAudio_SetAudioSourceAttenuationMode
typedef enum 
{
  ovrAudioSourceAttenuationMode_None          = 0,      ///< Sound is not attenuated, e.g. middleware handles attenuation
  ovrAudioSourceAttenuationMode_Fixed         = 1,      ///< Sound has fixed attenuation (passed to ovrAudio_SetAudioSourceAttenuationMode)
  ovrAudioSourceAttenuationMode_InverseSquare = 2,      ///< Sound uses internally calculated attenuation based on inverse square

  ovrAudioSourceAttenuationMode_COUNT
  
} ovrAudioSourceAttenuationMode;

/// Spatializer enumerant
///
/// \see ovrAudio_CreateContext
typedef enum
{
    ovrAudioSpatializationProvider_None                  = 0,       ///< Do not spatialization, dummy context just splits mono to L/R
    ovrAudioSpatializationProvider_OVR_Simple            = 1,       ///< (Compatibility only) Maps to OVR_OculusHQ internally
    ovrAudioSpatializationProvider_OVR_HQ                = 2,       ///< (Compatibility only) Maps to OVR_OculusHQ internally
    ovrAudioSpatializationProvider_OVR_OculusHQ          = 3,       ///< OculusHQ path w/ reflections and reverberation

    ovrAudioSpatializationProvider_COUNT
} ovrAudioSpatializationProvider;

/// Audio source properties (values)
///
/// \see ovraudio_SetAudioSourcePropertyf
typedef enum 
{
  ovrAudioSourceProperty_Diameter,       ///< Set virtual diameter of a spherical sound source. Default is a point sound source with no size. Larger sizes provide more envelopment for volumetric sounds. Diameter clamped to range 0..100.
  ovrAudioSourceProperty_MaxSpeed,       ///< Max speed, in meters/second, that this sound source can travel.  Any jump in motion larger than that will trigger a reset of positional interpolation state to avoid artifacts. Default is 0.0.
  
} ovrAudioSourceProperty;

/// Global boolean flags
///
/// \see ovrAudio_Enable
typedef enum
{
    ovrAudioEnable_None                     = 0,   ///< None
    ovrAudioEnable_Doppler_RESERVED         = 1,   ///< Global control of Doppler, default: disabled, current unimplemented
    ovrAudioEnable_SimpleRoomModeling       = 2,   ///< Enable/disable simple room modeling globally, default: disabled
    ovrAudioEnable_LateReverberation        = 3,   ///< Late reverbervation, requires simple room modeling enabled
    ovrAudioEnable_RandomizeReverb          = 4,   ///< Randomize reverbs to diminish artifacts.  Default: enabled.

    ovrAudioEnable_COUNT
} ovrAudioEnable;

/// Internal use only
///
/// Internal use only
typedef enum 
{

  ovrAudioHRTFInterpolationMethod_Nearest,
  ovrAudioHRTFInterpolationMethod_SimpleTimeDomain,
  ovrAudioHRTFInterpolationMethod_MinPhaseTimeDomain,
  ovrAudioHRTFInterpolationMethod_PhaseTruncation,
  ovrAudioHRTFInterpolationMethod_PhaseLerp,

  ovrAudioHRTFInterpolationMethod_COUNT

} ovrAudioHRTFInterpolationMethod;

/// Status mask returned by spatializer APIs
///
/// Mask returned from spatialization APIs consists of combination of these.
/// \see ovrAudio_SpatializeMonoSourceLR
/// \see ovrAudio_SpatializeMonoSourceInterleaved
typedef enum
{
  ovrAudioSpatializationStatus_None      = 0x00,  ///< Nothing to report
  ovrAudioSpatializationStatus_Finished  = 0x01,  ///< Buffer is empty and sound processing is finished
  ovrAudioSpatializationStatus_Working   = 0x02,  ///< Data still remains in buffer (e.g. reverberation tail)

} ovrAudioSpatializationStatus;

/// Spatialization flag
///
/// \see ovrAudio_SpatializeMonoSourceLR
/// \see ovrAudio_SpatializeMonoSourceInterleaved
typedef enum
{
  ovrAudioSpatializationFlag_None      = 0x00,  ///< normal

} ovrAudioSpatializationFlag;

/// Headphone models used for correction
///
/// \see ovrAudio_SetHeadphoneModel.
typedef enum
{
    ovrAudioHeadphones_None           = -1,  ///< No correction applied
    ovrAudioHeadphones_Rift           = 0,   ///< Apply correction for default headphones on Rift
    ovrAudioHeadphones_Rift_INTERNAL0 = 1,   ///< Apply correction for default headphones on Rift
    ovrAudioHeadphones_Rift_INTERNAL1 = 2,   ///< Apply correction for default headphones on Rift
    ovrAudioHeadphones_Rift_INTERNAL2 = 3,   ///< Apply correction for default headphones on Rift
    ovrAudioHeadphones_Rift_INTERNAL3 = 4,   ///< Apply correction for default headphones on Rift
    ovrAudioHeadphones_Rift_INTERNAL4 = 5,   ///< Apply correction for default headphones on Rift

    ovrAudioHeadphones_Custom         = 10,  ///< Apply correction using custom IR
    
    ovrAudioHeadphones_COUNT
} ovrAudioHeadphones;

/// Performance counter enumerants
///
/// \see ovrAudio_GetPerformanceCounter
/// \see ovrAudio_SetPerformanceCounter
typedef enum
{
    ovrAudioPerformanceCounter_Spatialization            = 0,  ///< Retrieve profiling information for spatialization
    ovrAudioPerformanceCounter_HeadphoneCorrection       = 1,  ///< Retrieve profiling information for headphone correction

    ovrAudioPerformanceCounter_COUNT
} ovrAudioPerformanceCounter;

/// Opaque type definitions for audio source and context
typedef struct ovrAudioSource_   ovrAudioSource;
typedef struct ovrAudioContext_ *ovrAudioContext;
typedef struct ovrAudioAmbisonicStream_ *ovrAudioAmbisonicStream;

/// Initialize OVRAudio
///
/// Load the OVR audio library.  Call this first before any other ovrAudio
/// functions!
///
/// \return Returns an ovrResult indicating success or failure.
///
/// \see ovrAudio_Shutdown
ovrResult ovrAudio_Initialize( void );

/// Shutdown OVRAudio
///
///  Shut down all of ovrAudio.  Be sure to destroy any existing contexts 
///  first!
///
///  \see ovrAudio_Initialize
///
void ovrAudio_Shutdown( void );

/// Return library's built version information.
///
/// Can be called any time.
/// \param[out] Major pointer to integer that accepts major version number
/// \param[out] Minor pointer to integer that accepts minor version number
/// \param[out] Patch pointer to integer that accepts patch version number
///
/// \return Returns a string with human readable build information
///
const char *ovrAudio_GetVersion( int *Major, int *Minor, int *Patch );

/// Allocate properly aligned buffer to store samples.
///
/// Helper function that allocates 16-byte aligned sample data sufficient
/// for passing to the spatialization APIs.
///
/// \param NumSamples number of samples to allocate 
/// \return Returns pointer to 16-byte aligned float buffer, or NULL on failure
/// \see ovrAudio_FreeSamples
///
OVRA_EXPORT float * ovrAudio_AllocSamples( int NumSamples );

/// Free previously allocated buffer
///
/// Helper function that frees 16-byte aligned sample data previously
/// allocated by ovrAudio_AllocSamples.
///
/// \param Samples pointer to buffer previously allocated by ovrAudio_AllocSamples
/// \see ovrAudio_AllocSamples
///
OVRA_EXPORT void ovrAudio_FreeSamples( float *Samples );

/// Retrieve a transformation from an ovrPosef.
///
/// \param Pose[in] pose to fetch transform from
/// \param Vx[out] buffer to store orientation vector X
/// \param Vy[out] buffer to store orientation vector Y
/// \param Vz[out] buffer to store orientation vector Z
/// \param Pos[out] buffer to store position
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_GetTransformFromPose( const ovrPosef *Pose, 
                                                     float *Vx, float *Vy, float *Vz,
                                                     float *Pos );

/// Audio context configuration structure
///
/// Passed to ovrAudio_CreateContext
///
/// \see ovrAudio_CreateContext
///
typedef struct _ovrAudioContextConfig
{
  uint32_t   acc_Size;              ///< set to size of the struct
  uint32_t   acc_Provider;          ///< should be one of ovrAudioSpatializationProvider
  uint32_t   acc_MaxNumSources;     ///< maximum number of audio sources to support
  uint32_t   acc_SampleRate;        ///< sample rate (16000 to 48000, but 44100 and 48000 are recommended for best quality)
  uint32_t   acc_BufferLength;      ///< number of samples in mono input buffers passed to spatializer
} ovrAudioContextConfiguration;

/// Create an audio context for spatializing incoming sounds.
///
/// Creates an audio context with the given configuration.
///
/// \param pContext[out] pointer to store address of context.  NOTE: pointer must be pointing to NULL!
/// \param pConfig[in] pointer to configuration struct describing the desired context attributes
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_DestroyContext
/// \see ovrAudioContextConfiguration
///
OVRA_EXPORT ovrResult ovrAudio_CreateContext( ovrAudioContext *pContext,
                                        const ovrAudioContextConfiguration *pConfig );

/// Destroy a previously created audio context.
///
/// \param[in] Context a valid audio context
/// \see ovrAudio_CreateContext
///
OVRA_EXPORT void ovrAudio_DestroyContext( ovrAudioContext Context );

/// Enable/disable options in the audio context.
///
/// \param Context context to use
/// \param What specific property to enable/disable
/// \param Enable 0 to disable, 1 to enable
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_Enable( ovrAudioContext Context, 
                                  ovrAudioEnable What, 
                                  int Enable );

/// Set HRTF interpolation method.
///
/// NOTE: Internal use only!
/// \param Context context to use
/// \param InterpolationMethod method to use
///
OVRA_EXPORT ovrResult ovrAudio_SetHRTFInterpolationMethod( ovrAudioContext Context, 
                                                      ovrAudioHRTFInterpolationMethod InterpolationMethod );

/// Box room parameters used by ovrAudio_SetSimpleBoxRoomParameters
///
/// \see ovrAudio_SetSimpleBoxRoomParameters
typedef struct _ovrAudioBoxRoomParameters
{
   uint32_t       brp_Size;                              ///< Size of struct
   float          brp_ReflectLeft, brp_ReflectRight;     ///< Reflection values (0..0.95)
   float          brp_ReflectUp, brp_ReflectDown;        ///< Reflection values (0..0.95)
   float          brp_ReflectBehind, brp_ReflectFront;   ///< Reflection values (0..0.95)
   float          brp_Width, brp_Height, brp_Depth;      ///< Size of box in meters
} ovrAudioBoxRoomParameters;

/// Set box room parameters for reverberation.
///
/// These parameters are used for reverberation/early reflections if 
/// ovrAudioEnable_SimpleRoomModeling is enabled.
///
/// Width/Height/Depth default is 11/10/9m
/// Reflection constants default to 0.25
///
/// \param Context[in] context to use
/// \param Parameters[in] pointer to ovrAudioBoxRoomParameters describing box
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudioBoxRoomParameters
/// \see ovrAudio_Enable
///
OVRA_EXPORT ovrResult ovrAudio_SetSimpleBoxRoomParameters( ovrAudioContext Context, 
                                                     const ovrAudioBoxRoomParameters *Parameters );

/// Sets the listener's pose state
///
/// If this is not set then the listener is always assumed to be facing into
/// the screen (0,0,-1) at location (0,0,0) and that all spatialized sounds
/// are in listener-relative coordinates.
///
/// \param Context[in] context to use
/// \param PoseState[in] listener's pose state as returned by LibOVR
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_SetListenerPoseStatef( ovrAudioContext Context, 
                                                const ovrPoseStatef *PoseState );

/// Reset an audio source's state.
///
/// Sometimes you need to reset an audio source's internal state due to a change
/// in the incoming sound or parameters.  For example, removing any reverb
/// tail since the incoming waveform has been swapped.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_ResetAudioSource( ovrAudioContext Context, int Sound );

/// Sets the position of an audio source.  Use "OVR" coordinate system (same as pose).
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param X position of sound on X axis
/// \param Y position of sound on Y axis
/// \param Z position of sound on Z axis
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_SetListenerPoseStatef
/// \see ovrAudio_SetAudioSourceRange
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourcePos( ovrAudioContext Context, 
                                                  int Sound, 
                                                  float X, float Y, float Z );

/// Sets the min and max range of the audio source.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param RangeMin min range in meters (full gain)
/// \param RangeMax max range in meters
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_SetListenerPoseStatef
/// \see ovrAudio_SetAudioSourcePos
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourceRange( ovrAudioContext Context, 
                                                    int Sound, 
                                                    float RangeMin, float RangeMax );

/// Sets an audio source's flags.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param Flags a logical OR of ovrAudioSourceFlag enumerants
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourceFlags( ovrAudioContext Context, 
                                                    int Sound, 
                                                    uint32_t Flags );

/// Sets a floating point property of an audio source.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param Property which property to set
/// \param Value value of the property
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourcePropertyf( ovrAudioContext Context, 
                                                        int Sound, 
                                                        ovrAudioSourceProperty Property, 
                                                        float Value );

/// Sets the direction of an audio source.  Use "OVR" coordinate system (same as pose).
///
/// This is an experimental feature only.  Vn should be a unit vector pointing AWAY from
/// the sound source.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param VX X component of direction vector
/// \param VY Y component of direction vector
/// \param VZ Z component of direction vector
/// \param Angle Cone angle of the sound source in degrees.  A value of 0 disables directionality.
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_SetListenerPoseStatef
/// \see ovrAudio_SetAudioSourceRange
/// \see ovrAudio_SetAudioSourcePos
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourceDirectionRESERVED( ovrAudioContext Context, 
                                                                int Sound, 
                                                                float VX, float VY, float VZ, 
                                                                float Angle );

/// Set the attenuation mode for a sound source.
///
/// Sounds can have their volume attenuated by distance based on different methods.
///
/// \param Context context to use
/// \param Sound index of sound (0..NumSources-1)
/// \param Mode attenuation mode to use
/// \param FixedScale attenuation constant used for fixed attenuation mode
/// \return Returns an ovrResult indicating success or failure
///
OVRA_EXPORT ovrResult ovrAudio_SetAudioSourceAttenuationMode( ovrAudioContext Context, 
                                                              int Sound, 
                                                              ovrAudioSourceAttenuationMode Mode, 
                                                              float FixedScale );

/// Spatialize a mono audio source to interleaved stereo output.
///
/// \param Context[in] context to use
/// \param Sound[in] index of sound (0..NumSources-1)
/// \param InFlags[in] spatialization flags to apply 
/// \param OutStatus[out] bitwise OR of flags indicating status of currently playing sound
/// \param Dst[out] pointer to stereo interleaved floating point destination buffer
/// \param Src[in] pointer to mono floating point buffer to spatialize
/// \return Returns an ovrResult indicating success or failure
///
/// \see ovrAudio_SpatializeMonoSourceLR
///
OVRA_EXPORT ovrResult ovrAudio_SpatializeMonoSourceInterleaved( ovrAudioContext Context, 
                                                          int Sound, 
                                                          uint32_t InFlags, 
                                                          uint32_t *OutStatus, 
                                                          float *Dst, const float *Src );

/// Spatialize a mono audio source to separate left and right output buffers.
///
/// \param Context[in] context to use
/// \param Sound[in] index of sound (0..NumSources-1)
/// \param InFlags[in] spatialization flags to apply 
/// \param OutStatus[out] bitwise OR of flags indicating status of currently playing sound
/// \param DstLeft[out]  pointer to floating point left channel buffer
/// \param DstRight[out] pointer to floating point right channel buffer
/// \param Src[in] pointer to mono floating point buffer to spatialize
/// \return Returns an ovrResult indicating success or failure
///
/// \see ovrAudio_SpatializeMonoSourceInterleaved
///
OVRA_EXPORT ovrResult ovrAudio_SpatializeMonoSourceLR( ovrAudioContext Context, 
                                                 int Sound, 
                                                 uint32_t InFlags, 
                                                 uint32_t *OutStatus, 
                                                 float *DstLeft, float *DstRight, 
                                                 const float *Src );

/// Set the headphone model used by the headphone correction algorithm.
///
/// \param Context[in] context to use
/// \param Model[in] model to use
/// \param ImpulseResponse[in] impulse response to use
/// \param NumSamples[in] size of impulse response in samples
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_ApplyHeadphoneCorrection
///
OVRA_EXPORT ovrResult ovrAudio_SetHeadphoneModel( ovrAudioContext Context, 
                                            ovrAudioHeadphones Model,
                                            const float *ImpulseResponse, int NumSamples );

/// Apply headphone correction algorithm to stereo buffer.
///
/// NOTE: Currently unimplemented
///
/// \param Context[in] context to use
/// \param OutLeft[out] pointer to floating point left channel buffer destination
/// \param OutRight[out] pointer to floating point right channel buffer destination
/// \param InLeft[in] pointer to floating point left channel buffer source
/// \param InRight[in] pointer to floating point right channel buffer source
/// \param NumSamples size of buffers (in samples)
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_SetHeadphoneModel
///
OVRA_EXPORT ovrResult ovrAudio_ApplyHeadphoneCorrection( ovrAudioContext Context, 
                                              float *OutLeft, float *OutRight,
                                              const float *InLeft, const float *InRight,
                                              int NumSamples );

/// \struct ovrAudioUserConfig
/// \brief  User config interface
///
typedef struct _ovrAudioUserConfig
{
   uint32_t   auc_Size;              // set to size of the struct
   uint32_t   auc_Reserved;          // just in case we need a flags value for params
   float      auc_HeadSize;          // head size (distance between ears) in cm.
} ovrAudioUserConfig;

/// Set user configuration.
///
/// NOTE: This API is intended to let you set user configuration parameters that
/// may assist with spatialization.
///
/// \param Context[in] context to use
/// \param Config[in] configuration state
OVRA_EXPORT ovrResult ovrAudio_SetUserConfig( ovrAudioContext Context, 
                                              const ovrAudioUserConfig *Config );

/// \typedef ovrAudioPrivateAPI
/// \brief   Opaque type used for access to private/hidden functions.
///
typedef struct _ovrAudioPrivateAPI ovrAudioPrivateAPI;

OVRA_EXPORT ovrResult ovrAudio_GetPrivateAPI( ovrAudioContext Context, ovrAudioPrivateAPI *Dst );

/// Retrieve a performance counter.
///
/// \param Context[in] context to use
/// \param Counter[in] the counter to retrieve
/// \param Count[out] destination for count variable (number of times that counter was updated)
/// \param TimeMicroSeconds destination for total time spent in that performance counter
/// \return Returns an ovrResult indicating success or failure
/// \see ovrAudio_ResetPerformanceCounter
///
OVRA_EXPORT ovrResult ovrAudio_GetPerformanceCounter( ovrAudioContext Context, 
                                                      ovrAudioPerformanceCounter Counter, 
                                                      int64_t *Count, 
                                                      double *TimeMicroSeconds );

/// Reset a performance counter.
///
/// \param Context[in] context to use
/// \param Counter[in] the counter to retrieve
/// \see ovrAudio_ResetPerformanceCounter
///
OVRA_EXPORT ovrResult ovrAudio_ResetPerformanceCounter( ovrAudioContext Context, 
                                                        ovrAudioPerformanceCounter Counter );

/// Quad-binaural spatialization 
///
/// \param ForwardLR[in] pointer to stereo interleaved floating point binaural audio for the forward direction (0 degrees)
/// \param RightLR[in] pointer to stereo interleaved floating point binaural audio for the right direction (90 degrees)
/// \param BackLR[in] pointer to stereo interleaved floating point binaural audio for the backward direction (180 degrees)
/// \param LeftLR[in] pointer to stereo interleaved floating point binaural audio for the left direction (270 degrees)
/// \param LookDirectionX[in] X component of the listener direction vector
/// \param LookDirectionY[in] Y component of the listener direction vector
/// \param LookDirectionZ[in] Z component of the listener direction vector
/// \param NumSamples[in] size of audio buffers (in samples)
/// \param Dst[out] pointer to stereo interleaved floating point destination buffer
///
OVRA_EXPORT ovrResult ovrAudio_ProcessQuadBinaural(const float *ForwardLR, const float *RightLR, const float *BackLR, const float *LeftLR,
                                                   float LookDirectionX, float LookDirectionY, float LookDirectionZ, 
                                                   int NumSamples, float *Dst);

/// Create an ambisonic stream instance for spatializing B-format ambisonic audio
///
/// \param SampleRate[in] sample rate of B-format signal (16000 to 48000, but 44100 and 48000 are recommended for best quality)
/// \param AudioBufferLength[in] size of audio buffers
/// \param pContext[out] pointer to store address of stream.
///
OVRA_EXPORT ovrResult ovrAudio_CreateAmbisonicStream(int SampleRate, int AudioBufferLength, ovrAudioAmbisonicStream* pAmbisonicStream);

/// Destroy a previously created ambisonic stream.
///
/// \param[in] Context a valid ambisonic stream
/// \see ovrAudio_CreateAmbisonicStream
///
OVRA_EXPORT ovrResult ovrAudio_DestroyAmbisonicStream(ovrAudioAmbisonicStream AmbisonicStream);

/// Spatialize ambisonic stream
///
/// \param Src[in] pointer to 4 channel interleaved B-format floating point buffer to spatialize
/// \param Dst[out] pointer to stereo interleaved floating point destination buffer
///
OVRA_EXPORT ovrResult ovrAudio_ProcessAmbisonicStreamInterleaved(ovrAudioAmbisonicStream AmbisonicStream, const float *Src, float *Dst, int NumSamples);

/// Set listener orientation for ambisonic stream
///
/// \param LookDirectionX[in] X component of the listener direction vector
/// \param LookDirectionY[in] Y component of the listener direction vector
/// \param LookDirectionZ[in] Z component of the listener direction vector
/// \param UpDirectionX[in] X component of the listener up vector
/// \param UpDirectionY[in] Y component of the listener up vector
/// \param UpDirectionZ[in] Z component of the listener up vector
///
OVRA_EXPORT ovrResult ovrAudio_SetAmbisonicListenerOrientation(ovrAudioAmbisonicStream AmbisonicStream,
                                                               float LookDirectionX, float LookDirectionY, float LookDirectionZ,
                                                               float UpDirectionX, float UpDirectionY, float UpDirectionZ);

// Testing interface
// Register custom HRTF dataset
typedef struct HRTFDataSet HRTFDataSet;
OVRA_EXPORT ovrResult ovrAudio_RegisterHRTFDataSet(const HRTFDataSet *dataSet, int *index);

// Switch data set
OVRA_EXPORT ovrResult ovrAudio_SetHRTFDataSetIndex(int index);

// Toggle randomization (in time) of reflections
OVRA_EXPORT ovrResult ovrAudio_SetReflectionRandomizationEnabled(int enabled);

#ifdef __cplusplus
}
#endif

/*! 

\section intro_sec Introduction

The OVRAudio API is a C/C++ interface that implements HRTF-based spatialization
and optional room effects. Your application can directly use this API, though 
most developers will access it indirectly via one of our plugins for popular 
middleware such as FMOD, Wwise, and Unity.  Starting with Unreal Engine 4.8 it
will also be available natively.

OVRAudio is a low-level API, and as such, it does not buffer or manage sound
state for applications. It positions sounds by filtering incoming monophonic 
audio buffers and generating floating point stereo output buffers. Your 
application must then mix, convert, and feed this signal to the appropriate 
audio output device.

OVRAudio does not handle audio subsystem configuration and output. It is up to 
developers to implement this using either a low-level system interface 
(e.g., DirectSound, WASAPI, CoreAudio, ALSA) or a high-level middleware 
package (e.g,  FMOD, Wwise, Unity). 

If you are unfamiliar with the concepts behind audio and virtual reality, we 
strongly recommend beginning with the companion guide 
*Introduction to Virtual Reality Audio*.

\section sysreq System Requirements

-Windows 7 and 8.x (32 and 64-bit)
-Android
-Mac OS X 10.9+

\section installation Installation

OVRAudio is distributed as a compressed archive. To install, unarchive it in 
your development tree and update your compiler include and lib paths 
appropriately.

When deploying an application on systems that support shared libraries, you 
must ensure that the appropriate DLL/shared library is in the same directory
as your application (Android uses static libraries).

\section multithreading Multithreading

OVRAudio does not create multiple threads  and uses a per-context mutex for 
safe read/write access via the API functions from different threads.  It is the
application's responsibility to coordinate context management between different 
threads.

\section using Using OVRAudio

This section covers the basics of using OVRAudio in your game or application.

\subsection Initialization

The following code sample illusrtates how to initial OVRAudio.  Note that there
are two distinct elements to initialization: general library initialization
(using ovrAudio_Initialize) and context creation (using ovrAudio_CreateContext).

ovrAudio_Initialize loads the shared libraries and performs one-time global
setup and initialization.

Contexts contain the state for a specific spatializer instance.  In most cases
you will only need a single context.

\code{.cpp}
#include "OVR_Audio.h"

OVRAudioContext context;

void setup()
{
    // Version checking is not strictly necessary but it's a good idea!
    int major, minor, patch;
    const char *VERSION_STRING;
     
    VERSION_STRING = ovrAudio_GetVersion( &major, &minor, &patch );
    printf( "Using OVRAudio: %s\n", VERSION_STRING );

    if ( major != OVR_AUDIO_MAJOR_VERSION || 
         minor != OVR_AUDIO_MINOR_VERSION )
    {
      printf( "Mismatched Audio SDK version!\n" );
    }

    // Initialize the library.  This finds the DLL, loads it, and then
    // maps the various entry points.
    if ( ovrAudio_Initialize() != ovrSuccess )
    {
      printf( "Could not initialize library!\n" );
      return;
    }

    ovrAudioContextConfiguration config = {};

    config.acc_Size = sizeof( config );
    config.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
    config.acc_SampleRate = 48000;
    config.acc_BufferLength = 512;
    config.acc_MaxNumSources = 16;

    ovrAudioContext context;

    if ( ovrAudio_CreateContext( &context, &config ) != ovrSuccess )
    {
      printf( "WARNING: Could not create context!\n" );
      return;
    }
}

\endcode

\subsection gflags Global Flags

A few global flags control OVRAudio's implementation.  These are managed with
ovrAudio_Enable and the appropriate flag:
- ovrAudioEnable_SimpleRoomModeling: Enables box room modeling of
reverberations and reflections
- ovrAudioEnable_LateReverberation: (Requires ovrAudioEnable_SimpleRoomModeling)
Splits room modeling into two components: early reflections (echoes) and late
reverberations.  Late reverberation may be independently disabled.
- ovrAudioEnable_RandomizeReverb: (requires ovrAudioEnable_SimpleRoomModeling
and ovrAudioEnable_LateReverberation) Randomizes reverberation tiles, creating
a more natural sound.

\subsection sourcemanagement Audio Source Management

OVRAudio maintains a set of N audio sources, where N is determined by the value
specified in ovrAudioContextConfiguration::acc_MaxNumSources passed to 
ovrAudio_CreateContext.

Each source is associated with a set of parameter, such as:
- position (ovrAudio_SetAudioSourcePosition)
- attenuation range (ovrAudio_SetAudioSourceRange)
- flags (ovrAudio_SetAudioSourceFlags)
- attenuation mode (ovrAudio_SetAudioSourceAttenuationMode)

These may be changed at any time prior to a call to the spatialization APIs.  The
source index (0..N-1) is a parameter to the above functions.

Note: Some lingering states such as late reverberation tails may carry over between
calls to the spatializer.  If you dynamically change source sounds bound to an audio
source (for example, you have a pool of OVRAudio sources), you will need to call
ovrAudio_ResetAudioSource to avoid any artifacts.

\subsection attenuation Attenuation

The volume of a sound is attenuated over distance, and this can be modeled in
different ways.  By default, OVRAudio does not perform any attenuation, as the most
common use case is an application or middleware defined attenuation curve.

If you want OVRAudio to attenuate volume based on distance between the sound 
source and listener, call ovrAudio_SetAudioSourceAttenuationMode with the
the appropriate mode.  OVRAudio can also scale volume by a fixed value using
ovrAudioSourceAttenuationMode_Fixed.  If, for example, you have computed the
attenuation factor and would like OVRAudio to apply it during spatialization.

\subsection sourceflags Audio Source Flags

You may define properties of specific audio sources by setting appropriate flags
using the ovrAudio_SetAudioSourceFlags aPI.  These flags include:
- ovrAudioSourceFlag_WideBand_HINT: Set this to help mask certain artifacts for 
wideband audio sources with a lot of spectral content, such as music, voice and
noise.
- ovrAudioSourceFlag_NarrowBand_HINT: Set this for narrowband audio sources that
lack broad spectral content such as pure tones (sine waves, whistles).
- ovrAudioSourceFlag_DirectTimeOfArrival: Simulate travel time for a sound.  This
is physicaly correct, but may be perceived as less realistic, as games and media
commonly represent sound travel as instantaneous.

\subsection sourcesize Audio Source Size

OVRAudio treats sound sources as infinitely small point sources by default.  
This works in most cases, but when a source approaches the listener it may sound
incorret, as if the sound were coming from between the listener's ears.  You may
set a virtual dieamater for a sound source using ovrAudio_SetAudioSourcePropertyf
with the flag ovrAudioSourceProperty_Diameter.  As the listener enters the
sphere, the sounds seems to come from a wider area surrounding the listener's
head.

\subsection envparm Set Environmental Parameters

As the listener transitions into different environments, you can reconfigure the
environment effects parameters.  You may begin by simply inheriting the default
values or setting the values globally a single time.

NOTE: Reflections/reverberation must be enabled

\code
void enterRoom( float w, float h, float d, float r )
{
     ovrAudioBoxRoomParameters brp = {};

     brp.brp_Size = sizeof( brp );
     brp.brp_ReflectLeft = brp.brp_ReflectRight =
     brp.brp_ReflectUp = brp.brp_ReflectDown =
     brp.brp_ReflectFront = brp.brp_ReflectBehind = r;

     brp.brp_Width = w;
     brp.brp_Height = h;
     brp.brp_Depth = d;

     ovrAudio_SetSimpleBoxRoomParameters( context, &brp );
}  
\endcode

\subsection headtracking Head Tracking (Optional)

You may specify the listener's pose state using values retrieved directly from 
the HMD using LibOVR:
\code
void setListenerPose( const ovrPoseStatef *PoseState )
{
   ovrAudio_SetListenerPoseStatef( AudioContext, PoseState );
}
\endcode

All sound sources are transformed with reference to the specified pose so that
they remain positioned correctly relative to the listener. If you do not call 
this function, the listener is assumed to be at (0,0,0) and looking forward 
(down the -Z axis), and that all sounds are in listener-relative coordinates.

\subsection spatialization Applying 3D Spatialization

Applying 3D spatialiazation consists of looping over all of your sounds, 
copying their data into intermediate buffers, and passing them to the 
positional audio engine. It will in turn process the sounds with the 
appropriate HRTFs and effects and return a floating point stereo buffer:
\code
void processSounds( Sound *sounds, int NumSounds, float *MixBuffer )
{
   // This assumes that all sounds want to be spatialized!
   // NOTE: In practice these should be 16-byte aligned, but for brevity
   // we're just showing them declared like this
   uint32_t Flags = 0, Status = 0;

   float outbuffer[ INPUT_BUFFER_SIZE * 2 ];
   float inbuffer[ INPUT_BUFFER_SIZE };

   for ( int i = 0; i < NumSounds; i++ )
   {
      // Set the sound's position in space (using OVR coordinates)
      // NOTE: if a pose state has been specified by a previous call to
      // ovrAudio_ListenerPoseStatef then it will be transformed 
      // by that as well
      ovrAudio_SetAudioSourcePos( AudioContext, i, 
         sounds[ i ].X, sounds[ i ].Y, sounds[ i ].Z );

      // This sets the attenuation range from max volume to silent
      // NOTE: attenuation can be disabled or enabled
       ovrAudio_SetAudioSourceRange( AudioContext, i, 
         sounds[ i ].RangeMin, sounds[ i ].RangeMax );

      // Grabs the next chunk of data from the sound, looping etc. 
      // as necessary.  This is application specific code.
      sounds[ i ].GetSoundData( inbuffer );

      // Spatialize the sound into the output buffer.  Note that there
      // are two APIs, one for interleaved sample data and another for
      // separate left/right sample data
      ovrAudio_SpatializeMonoSourceInterleaved( AudioContext, 
          i, 
         Flags, &Status,
         outbuffer, inbuffer );

      // Do some mixing      
      for ( int j = 0; j < INPUT_BUFFER_SIZE; j++ )
      {
         if ( i == 0 )
         {
            MixBuffer[ j ] = outbuffer[ j ];
         }
         else
         {
            MixBuffer[ j ] += outbuffer[ j ];
         }
      }   
   }

   // From here we'd send the MixBuffer on for more processing or
   // final device output

   PlayMixBuffer( ... );
}
\endcode

At that point we have spatialized sound mixed into our output buffer.

\subsection reverbtails Finishing Reverb Tails

If late reverberation and simple box room modeling are enabled then the
actual output sound will have a duration longer than the incoming sound
(since it has to factor in the direct sound and then the additional echoes and
reverberation).  To ensure that these reverberation tails are not cut off, you
must feed the spatiailization functions with silence (e.g. NULL source data)
after your initial sample is processed and wait until the status returned is
ovrAudioSpatializationStatus_Finished.

\subsection headphone Headphone Correction

NOTE: This is currently not implemented!

All transducers (speakers, headphones, microphones, et cetera) introduce their
own characteristics on signals. This is usually measured as an **impulse 
response**. If we know the impulse response for a given transducer, we can correct
for this by applying its inverse through deconvolution. Because spatialization is 
highly dependent on frequency response, removing the transducer contribution from 
signals generally helps with spatialization.

The Audio SDK provides an optional set of APIs to perform this deconvolution. It 
requires having an impulse response for your headphones. For Rift, we have provided 
the impulse response internally.

void AdjustStereoForHeadphones( ovrAudioContext Context,
        float *OutLeft, float *OutRight,
        const float *InLeft, const float *InRight,
        int Length )
{
   // Normally we'd set headphone model just once
   // Note: IR is supplied for Rift
   ovrAudio_SetHeadphoneModel( Context, ovrAudioHeadphones_Rift, NULL, 0 );
   ovrAudio_HeadphoneCorrection( Context, 
      OutLeft, OutRight, 
      InLeft, InRight, 
      Length );
}

This correction should occur at the final output stage. In other words, it should be
applied directly on the stereo outputs and not on each sound.

\subsection profiling Profiling Performance (optional)

There is generally a trade-off between accuracy and performance, and spatialization
and room effects come at a cost. The performance impact for sound spatialization 
can be queried using OVRAudio's profiler API:

\code
void DumpStats( ovrAudioContext Context )
{
   int64_t Count;
   double TimeMicroSeconds;

   ovrAudio_GetPerformanceCounter( Context,
      ovrAudioPerformanceCounter_Spatialization,
      &Count, 
      &TimeMicroSeconds );

   LOG("Perf (spatialization): %lld calls %.02f us/call\n", 
      Count, 
      ( float ) ( TimeMicroSeconds/Count) );

   // optionally reset
   ovrAudio_ResetPerformanceCounter( Context,
      ovrAudioPerformanceCounter_Spatialization );
}
\endcode

\subsection shutdown Shutdown

When you are no longer using OVRAudio, shut it down by destroying any contexts, 
and then calling ovrAudio_Shutdown.

\code
void shutdownOvrAudio()
{
   ovrAudio_DestroyContext( &AudioContext );
   ovrAudio_Shutdown();
}
\endcode

*/

#endif // OVR_Audio_h
