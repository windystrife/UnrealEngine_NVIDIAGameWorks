#ifndef NV_CO_RENDER_CONTEXT_H
#define NV_CO_RENDER_CONTEXT_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Core/1.0/NvResult.h>
#include <Nv/Common/NvCoApiHandle.h>

#include <Nv/Common/Math/NvCoMathTypes.h>

#include <DirectXMath.h>

namespace nvidia {
namespace Common {

struct RenderContextOptions
{
	RenderContextOptions() :
		m_numMsaaSamples(0),
		m_msaaQuality(0),
		m_fullSpeed(false),
		m_allowFullScreen(true),
		m_enableProfile(false),
		m_useWarpDevice(false),
		m_numBackBuffers(0),
		m_numRenderFrames(0)
	{
	}

	Int m_numMsaaSamples;		///< Number of multi sample samples wanted
	Int m_msaaQuality;			///< The multi sampling quality

	Bool m_fullSpeed;					///< If set will try and render as fast as possible
	Bool m_allowFullScreen;				///< If set can go into full screen mode
	
	Bool m_enableProfile;				///< If true adds simple profiling
	Bool m_useWarpDevice;				///< True if warp device is wanted
	
	Int m_numBackBuffers;				///< If 0 do the default
	Int m_numRenderFrames;				///< Number of render flames allowed in flight, 0 does the default
};

class RenderContextListener
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(RenderContextListener)

		/// Called when work is submitted to the GPU
	virtual Void onGpuWorkSubmitted(const ApiHandle& handle) { NV_UNUSED(handle); }
};

class RenderContext 
{	
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(RenderContext)

		/// ScopeGpuWork is used to handle gpu work that must be synchronized with the CPU.
		/// It is guaranteed when scope is released the CPU and GPU will sync. NOTE! This means 
		/// you don't want to do this as part of render/update unless you have to because it will be slow.
	struct ScopeGpuWork
	{
		NV_FORCE_INLINE ScopeGpuWork(RenderContext* context) :
			m_context(context)
		{
			context->beginGpuWork();
		}
		NV_FORCE_INLINE ~ScopeGpuWork()
		{
			m_context->endGpuWork();
		}
	private:
		RenderContext* m_context;
	};

	struct ScopeRender
	{
		NV_FORCE_INLINE ScopeRender(RenderContext* context) :
			m_context(context)
		{
			context->beginRender();
		}
		NV_FORCE_INLINE ~ScopeRender()
		{
			m_context->endRender();
		}
	private:
		RenderContext* m_context;
	};

	/// Templated method to get Api interface
	template <typename T>
	T* getInterface() { return reinterpret_cast<T*>(getInterface(EApiType(T::API_TYPE))); }

	/// Given an api type get an interface for that type. Return NV_NULL if not supported.
	virtual Void* getInterface(EApiType apiType) = 0;

		/// Called to inform that underlying window size has changed. Will recreate buffers
	virtual void onSizeChanged(Int width, Int height, Bool minimized) = 0;

		/// Blocks until all submitted Gpu work has completed
	virtual void waitForGpu() = 0;

		/// Submits any currently outstanding gpu work - does not wait for it to complete and sync with CPU (like endCpuWork)
	virtual void submitGpuWork() = 0;

		/// If there is pending GPU work, that must be managed by the client, will block until work is completed
		/// on the endGpuWork call. Can be used to perform GPU work outside of beginRender/endRender cycle.
		/// Because of the matching requirement typically called through using RAII type ScopeGpuWork.
	virtual void beginGpuWork() = 0;
		/// Should match up with startGpuWork call. May block if outside of beginRender/endRender
	virtual void endGpuWork() = 0;
	 
		/// Should be called before any rendering calls take place. Is matched with endRender. 
	virtual void beginRender() = 0;
		/// Makes the display the current rendering target
	virtual void prepareRenderTarget() = 0;
		/// Clear the current render target to the specified color
	virtual void clearRenderTarget(const AlignedVec4* clearColorRgba) = 0;
		/// Called to complete rendering before 'present'. Should match with previous beginRender. 
	virtual void endRender() = 0;

		/// Shows the contents on the display
	virtual void present() = 0;

		/// Toggles the current display to full screen mode
	virtual Result toggleFullScreen() = 0;
		/// Returns true if currently in a full screen mode
	virtual Bool isFullScreen() = 0;

		/// Must be called before any other functionality can be used. If returns a failure, context cannot be used and must be deleted.
	virtual Result initialize(const RenderContextOptions& options, Void* windowsHandle);
	
		/// The width of the display
	Int getWidth() const { return m_width; }
		/// The height of the display
	Int getHeight() const { return m_height; }
		/// Get the aspect ratio
	NV_FORCE_INLINE Float getAspectRatio() const { return m_aspectRatio; }

		/// Get the currently set listener
	RenderContextListener* getListener() const { return m_listener; }
		/// Set the listener
	Void setListener(RenderContextListener* listener) { m_listener = listener; }

		/// Get the clear color
	const AlignedVec4& getClearColor() const { return m_clearColor; }
		/// Set the clear color
	Void setClearColor(const AlignedVec4& col) { m_clearColor = col; }

		/// Get the options
	const RenderContextOptions& getOptions() const { return m_options; }

		/// Ctor
	RenderContext(Int width, Int height, RenderContextListener* listener = NV_NULL);

protected:
	
	void updateForSizeChange(Int clientWidth, Int clientHeight);

	// Viewport dimensions.
	Int m_width;
	Int m_height;
	Float m_aspectRatio;						///< Aspect ratio of the display (width/height)

	AlignedVec4 m_clearColor;						///< The default clear color

	RenderContextOptions m_options;				///< The options applied to set up this context 
	RenderContextListener* m_listener;			///< A listener for events from the context. Can be null.
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_RENDER_CONTEXT_H