// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Layout/SlateRotatedRect.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/NavigationReply.h"
#include "Input/PopupMethodReply.h"

class FSlateInstanceBufferUpdate;
class FWidgetStyle;
class SWidget;
struct Rect;

#define SLATE_USE_32BIT_INDICES !PLATFORM_USES_ES2

#if SLATE_USE_32BIT_INDICES
typedef uint32 SlateIndex;
#else
typedef uint16 SlateIndex;
#endif

/**
 * Draw primitive types                   
 */
namespace ESlateDrawPrimitive
{
	typedef uint8 Type;

	const Type LineList = 0;
	const Type TriangleList = 1;
};

/**
 * Shader types. NOTE: mirrored in the shader file   
 * If you add a type here you must also implement the proper shader type (TSlateElementPS).  See SlateShaders.h
 */
namespace ESlateShader
{
	typedef uint8 Type;
	/** The default shader type.  Simple texture lookup */
	const Type Default = 0;
	/** Border shader */
	const Type Border = 1;
	/** Font shader, same as default except uses an alpha only texture */
	const Type Font = 2;
	/** Line segment shader. For drawing anti-aliased lines */
	const Type LineSegment = 3;
	/** For completely customized materials.  Makes no assumptions on use*/
	const Type Custom = 4;
	/** For post processing passes */
	const Type PostProcess = 5;
};

/**
 * Effects that can be applied to elements when rendered.
 * Note: New effects added should be in bit mask form
 * If you add a type here you must also implement the proper shader type (TSlateElementPS).  See SlateShaders.h
 */
enum class ESlateDrawEffect : uint8
{
	/** No effect applied */
	None					= 0,
	/** Advanced: Draw the element with no blending */
	NoBlending			= 1 << 0,
	/** Advanced: Blend using pre-multiplied alpha. Ignored if NoBlending is set. */
	PreMultipliedAlpha	= 1 << 1,
	/** Advanced: No gamma correction should be done */
	NoGamma				= 1 << 2,
	/** Advanced: Change the alpha value to 1 - Alpha. */
	InvertAlpha			= 1 << 3,

	// ^^ These Match ESlateBatchDrawFlag ^^

	/** Disables pixel snapping */
	NoPixelSnapping		= 1 << 4,
	/** Draw the element with a disabled effect */
	DisabledEffect		= 1 << 5,
	/** Advanced: Don't read from texture alpha channel */
	IgnoreTextureAlpha	= 1 << 6,

	/** Advanced: Existing Gamma correction should be reversed */
	ReverseGamma			= 1 << 7
};

ENUM_CLASS_FLAGS(ESlateDrawEffect);

/** Flags for drawing a batch */
enum class ESlateBatchDrawFlag : uint8
{
	/** No draw flags */
	None					= 0,
	/** Draw the element with no blending */
	NoBlending			= 1 << 0,
	/** Blend using pre-multiplied alpha. Ignored if NoBlending is set. */
	PreMultipliedAlpha	= 1 << 1,
	/** No gamma correction should be done */
	NoGamma				= 1 << 2,
	/** Change the alpha value to 1 - Alpha */
	InvertAlpha			= 1 << 3,

	// ^^ These Match ESlateDrawEffect ^^

	/** Draw the element as wireframe */
	Wireframe			= 1 << 4,
	/** The element should be tiled horizontally */
	TileU				= 1 << 5,
	/** The element should be tiled vertically */
	TileV				= 1 << 6,
	/** Reverse gamma correction */
	ReverseGamma			 = 1 << 7
};

ENUM_CLASS_FLAGS(ESlateBatchDrawFlag);

enum class ESlateLineJoinType : uint8
{
	// Joins line segments with a sharp edge (miter)
	Sharp =	0,
	// Simply stitches together line segments
	Simple = 1,
};



enum class ESlateVertexRounding : uint8
{
	Disabled,
	Enabled
};



/** 
 * A struct which defines a basic vertex seen by the Slate vertex buffers and shaders
 */
struct SLATECORE_API FSlateVertex
{
	/** Texture coordinates.  The first 2 are in xy and the 2nd are in zw */
	float TexCoords[4]; 

	/** Texture coordinates used as pass through to materials for custom texturing. */
	FVector2D MaterialTexCoords;

	/** Position of the vertex in window space */
	FVector2D Position;

	/** Vertex color */
	FColor Color;
	
	/** Local size of the element */
	uint16 PixelSize[2];

	FSlateVertex() {}
	
public:

	template<ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FVector2D& InTexCoord2, const FColor& InColor)
	{
		FSlateVertex Vertex;
		Vertex.TexCoords[0] = InTexCoord.X;
		Vertex.TexCoords[1] = InTexCoord.Y;
		Vertex.TexCoords[2] = InTexCoord2.X;
		Vertex.TexCoords[3] = InTexCoord2.Y;
		Vertex.InitCommon<Rounding>(RenderTransform, InLocalPosition, InColor);

		return Vertex;
	}

	template<ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FColor& InColor)
	{
		FSlateVertex Vertex;
		Vertex.TexCoords[0] = InTexCoord.X;
		Vertex.TexCoords[1] = InTexCoord.Y;
		Vertex.TexCoords[2] = 1.0f;
		Vertex.TexCoords[3] = 1.0f;
		Vertex.InitCommon<Rounding>(RenderTransform, InLocalPosition, InColor);

		return Vertex;
	}

	template<ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector4& InTexCoords, const FVector2D& InMaterialTexCoords, const FColor& InColor)
	{
		FSlateVertex Vertex;
		Vertex.TexCoords[0] = InTexCoords.X;
		Vertex.TexCoords[1] = InTexCoords.Y;
		Vertex.TexCoords[2] = InTexCoords.Z;
		Vertex.TexCoords[3] = InTexCoords.W;
		Vertex.MaterialTexCoords = InMaterialTexCoords;
		Vertex.InitCommon<Rounding>(RenderTransform, InLocalPosition, InColor);

		return Vertex;
	}

	template<ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InLocalSize, float Scale, const FVector4& InTexCoords, const FColor& InColor)
	{
		FSlateVertex Vertex;
		Vertex.TexCoords[0] = InTexCoords.X;
		Vertex.TexCoords[1] = InTexCoords.Y;
		Vertex.TexCoords[2] = InTexCoords.Z;
		Vertex.TexCoords[3] = InTexCoords.W;
		Vertex.MaterialTexCoords = FVector2D(InLocalPosition.X / InLocalSize.X, InLocalPosition.Y / InLocalSize.Y);
		Vertex.InitCommon<Rounding>(RenderTransform, InLocalPosition, InColor);
		Vertex.PixelSize[0] = FMath::RoundToInt(InLocalSize.X * Scale);
		Vertex.PixelSize[1] = FMath::RoundToInt(InLocalSize.Y * Scale);
		Vertex.MaterialTexCoords = FVector2D(InLocalPosition.X / InLocalSize.X, InLocalPosition.Y / InLocalSize.Y);

		return Vertex;
	}

private:

	template<ESlateVertexRounding Rounding>
	FORCEINLINE void InitCommon(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FColor& InColor)
	{
		Position = TransformPoint(RenderTransform, InLocalPosition);

		if ( Rounding == ESlateVertexRounding::Enabled )
		{
			Position.X = FMath::RoundToInt(Position.X);
			Position.Y = FMath::RoundToInt(Position.Y);
		}

		Color = InColor;
	}
};

template<> struct TIsPODType<FSlateVertex> { enum { Value = true }; };

/** Stores an aligned rect as shorts. */
struct FShortRect
{
	FShortRect()
		: Left(0)
		, Top(0)
		, Right(0)
		, Bottom(0)
	{
	}
	
	FShortRect(uint16 InLeft, uint16 InTop, uint16 InRight, uint16 InBottom)
		: Left(InLeft)
		, Top(InTop)
		, Right(InRight)
		, Bottom(InBottom)
	{
	}
	
	explicit FShortRect(const FSlateRect& Rect)
		: Left((uint16)FMath::Clamp(Rect.Left, 0.0f, 65535.0f))
		, Top((uint16)FMath::Clamp(Rect.Top, 0.0f, 65535.0f))
		, Right((uint16)FMath::Clamp(Rect.Right, 0.0f, 65535.0f))
		, Bottom((uint16)FMath::Clamp(Rect.Bottom, 0.0f, 65535.0f))
	{
	}

	bool operator==(const FShortRect& RHS) const { return Left == RHS.Left && Top == RHS.Top && Right == RHS.Right && Bottom == RHS.Bottom; }
	bool operator!=(const FShortRect& RHS) const { return !(*this == RHS); }
	bool DoesIntersect( const FShortRect& B ) const
	{
		const bool bDoNotOverlap =
			B.Right < Left || Right < B.Left ||
			B.Bottom < Top || Bottom < B.Top;

		return ! bDoNotOverlap;
	}

	bool DoesIntersect(const FSlateRect& B) const
	{
		const bool bDoNotOverlap =
			B.Right < Left || Right < B.Left ||
			B.Bottom < Top || Bottom < B.Top;

		return !bDoNotOverlap;
	}

	FVector2D GetTopLeft() const { return FVector2D(Left, Top); }
	FVector2D GetBottomRight() const { return FVector2D(Right, Bottom); }

	uint16 Left;
	uint16 Top;
	uint16 Right;
	uint16 Bottom;
};

static FVector2D RoundToInt(const FVector2D& Vec)
{
	return FVector2D(FMath::RoundToInt(Vec.X), FMath::RoundToInt(Vec.Y));
}

/**
 * Viewport implementation interface that is used by SViewport when it needs to draw and processes input.                   
 */
class ISlateViewport
{
public:
	virtual ~ISlateViewport() {}

	/**
	 * Called by Slate when the viewport widget is drawn
	 * Implementers of this interface can use this method to perform custom
	 * per draw functionality.  This is only called if the widget is visible
	 *
	 * @param AllottedGeometry	The geometry of the viewport widget
	 */
	virtual void OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) { }
	
	/**
	 * Returns the size of the viewport                   
	 */
	virtual FIntPoint GetSize() const = 0;

	/**
	 * Returns a slate texture used to draw the rendered viewport in Slate.                   
	 */
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const = 0;

	/**
	 * Does the texture returned by GetViewportRenderTargetTexture only have an alpha channel?
	 */
	virtual bool IsViewportTextureAlphaOnly() const
	{
		return false;
	}

	/**
	 * Performs any ticking necessary by this handle                   
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime )
	{
	}

	/**
	 * Returns true if the viewport should be vsynced.
	 */
	virtual bool RequiresVsync() const = 0;

	/**
	 * Whether the viewport contents should be scaled or not. Defaults to true.
	 */
	virtual bool AllowScaling() const
	{
		return true;
	}

	/**
	 * Called when Slate needs to know what the mouse cursor should be.
	 * 
	 * @return FCursorReply::Unhandled() if the event is not handled; FCursorReply::Cursor() otherwise.
	 */
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
	{
		return FCursorReply::Unhandled();
	}

	/**
	 * After OnCursorQuery has specified a cursor type the system asks each widget under the mouse to map that cursor to a widget. This event is bubbled.
	 * 
	 * @return TOptional<TSharedRef<SWidget>>() if you don't have a mapping otherwise return the Widget to show.
	 */
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply)
	{
		return TOptional<TSharedRef<SWidget>>();
	}

	/**
	 *	Returns whether the software cursor is currently visible
	 */
	virtual bool IsSoftwareCursorVisible() const
	{
		return false;
	}

	/**
	 *	Returns the current position of the software cursor
	 */
	virtual FVector2D GetSoftwareCursorPosition() const
	{
		return FVector2D::ZeroVector;
	}

	/**
	 * Called by Slate when a mouse button is pressed inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a mouse button is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}


	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{

	}

	virtual void OnMouseLeave( const FPointerEvent& MouseEvent )
	{
		
	}

	/**
	 * Called by Slate when a mouse button is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when the mouse wheel is used inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when the mouse wheel is used inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a key is pressed inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the key event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a key is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the key event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when an analog value changes on a button that supports analog
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InAnalogInputEvent Analog input event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a character key is pressed while the viewport has focus
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the character that was pressed
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when the viewport gains keyboard focus.  
	 *
	 * @param InFocusEvent	Information about what caused the viewport to gain focus
	 */
	virtual FReply OnFocusReceived( const FFocusEvent& InFocusEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is started (finger down)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is moved  (finger moved)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is ended (finger lifted)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called on a touchpad gesture event
	 *
	 * @param InGestureEvent	The touch event generated
	 */
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent )
	{
		return FReply::Unhandled();
	}
	
	/**
	 * Called when motion is detected (controller or device)
	 * e.g. Someone tilts or shakes their controller.
	 * 
	 * @param InMotionEvent	The motion event generated
	 */
	virtual FReply OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called to determine if we should render the focus brush.
	 *
	 * @param InFocusCause	The cause of focus
	 */
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const
	{
		return TOptional<bool>();
	}

	/**
	 * Called after all input for this frame is processed.
	 */
	virtual void OnFinishedPointerInput()
	{
	}

	/**
	 * Called to figure out whether we can make new windows for popups within this viewport.
	 * Making windows allows us to have popups that go outside the parent window, but cannot
	 * be used in fullscreen and do not have per-pixel alpha.
	 */
	virtual FPopupMethodReply OnQueryPopupMethod() const
	{
		return FPopupMethodReply::Unhandled();
	}

	/**
	 * Called when navigation is requested
	 * e.g. Left Joystick, Direction Pad, Arrow Keys can generate navigation events.
	 * 
	 * @param InNavigationEvent	The navigation event generated
	 */
	virtual FNavigationReply OnNavigation( const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent )
	{
		return FNavigationReply::Stop();
	}

	/**
	 * Give the viewport an opportunity to override the navigation behavior.
	 * This is called after all the navigation event bubbling is complete and we know a destination.
	 *
	 * @param InDestination	The destination widget
	 * @return whether we handled the navigation
	 */
	virtual bool HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination)
	{
		return false;
	}

	/**
	 * Called when the viewport loses keyboard focus.  
	 *
	 * @param InFocusEvent	Information about what caused the viewport to lose focus
	 */
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent )
	{
	}

	/**
	 * Called when the top level window associated with the viewport has been requested to close.
	 * At this point, the viewport has not been closed and the operation may be canceled.
	 * This may not called from PIE, Editor Windows, on consoles, or before the game ends
 	 * from other methods.
	 * This is only when the platform specific window is closed.
	 *
	 * @return FReply::Handled if the close event was consumed (and the window should remain open).
	 */
	virtual FReply OnRequestWindowClose()
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when the viewport has been requested to close.
	 */
	virtual void OnViewportClosed()
	{
	}

	/**
	 * Gets the SWidget associated with this viewport
	 */
	virtual TWeakPtr<SWidget> GetWidget()
	{
		return nullptr;
	}

	/**
	 * Called when the viewports top level window is being Activated
	 */
	virtual FReply OnViewportActivated(const FWindowActivateEvent& InActivateEvent)
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when the viewports top level window is being Deactivated
	 */
	virtual void OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent)
	{
	}
};

/**
 * An interface for a custom slate drawing element
 * Implementers of this interface are expected to handle destroying this interface properly when a separate 
 * rendering thread may have access to it. (I.E this cannot be destroyed from a different thread if the rendering thread is using it)
 */
class ICustomSlateElement
{
public:
	virtual ~ICustomSlateElement() {}

	/** 
	 * Called from the rendering thread when it is time to render the element
	 *
	 * @param RenderTarget	handle to the platform specific render target implementation.  Note this is already bound by Slate initially 
	 */
	virtual void DrawRenderThread(class FRHICommandListImmediate& RHICmdList, const void* RenderTarget) = 0;
};

/**
 * Represents a per instance data buffer for a custom Slate mesh element.
 * Use FSlateInstanceBufferUpdate to update the per-instance data.
 *  e.g.
 *    TSharedRef<FSlateInstanceBufferUpdate> NewUpdate = InstanceBuffer.BeginUpdate();
 *     NewUpdate.GetData().Add( FVector4(1,1,1,1) )
 *     FSlateInstanceBufferUpdate::CommitUpdate(NewUpdate);
 */
class ISlateUpdatableInstanceBuffer
{
public:
	virtual ~ISlateUpdatableInstanceBuffer(){};
	friend class FSlateInstanceBufferUpdate;

   /**
	* Use this method to begin a new update to this instance of the buffer:
	*/
	virtual TSharedPtr<class FSlateInstanceBufferUpdate> BeginUpdate() = 0;

	/** How many instances should we draw? */
	virtual uint32 GetNumInstances() const = 0;

private:
	friend class FSlateInstanceBufferUpdate;

	/** Updates rendering data for the GPU */
	virtual void UpdateRenderingData(int32 NumInstancesToUse) = 0;

	/** @return an array of instance data that is safe to populate (e.g not in use by the renderer) */
	virtual TArray<FVector4>& GetBufferData() = 0;
};

/** Represents an update to the per-instance buffer. */
class FSlateInstanceBufferUpdate
{
public:
	/** Access the per-instance data for modiciation */
	FORCEINLINE TArray<FVector4>& GetData(){ return Data; }
	
	/** Send an update to the render thread */
	static void CommitUpdate(TSharedPtr<FSlateInstanceBufferUpdate>& UpdateToCommit)
	{
		ensure(UpdateToCommit.GetSharedReferenceCount() == 1);
		UpdateToCommit->CommitUpdate_Internal();
		UpdateToCommit.Reset();
	}

	~FSlateInstanceBufferUpdate()
	{
		if (!bWasCommitted)
		{
			CommitUpdate_Internal();
		}
	}

private:
	friend class FSlateUpdatableInstanceBuffer;
	FSlateInstanceBufferUpdate(ISlateUpdatableInstanceBuffer& InBuffer)
		: Buffer(InBuffer)
		, Data(InBuffer.GetBufferData())
		, InstanceCount(0)
		, bWasCommitted(false)
	{
	}

	void CommitUpdate_Internal()
	{
		Buffer.UpdateRenderingData(Data.Num());
		bWasCommitted = true;
	}

	ISlateUpdatableInstanceBuffer& Buffer;
	TArray<FVector4>& Data;
	uint32 InstanceCount;
	bool bWasCommitted;
};
