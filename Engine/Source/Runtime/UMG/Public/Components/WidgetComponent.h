// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Layout/ArrangedWidget.h"
#include "WorldCollision.h"
#include "Components/MeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "WidgetComponent.generated.h"

class FHittestGrid;
class FPrimitiveSceneProxy;
class FWidgetRenderer;
class SVirtualWindow;
class SWindow;
class UBodySetup;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class EWidgetSpace : uint8
{
	/** The widget is rendered in the world as mesh, it can be occluded like any other mesh in the world. */
	World,
	/** The widget is rendered in the screen, completely outside of the world, never occluded. */
	Screen
};

UENUM(BlueprintType)
enum class EWidgetTimingPolicy : uint8
{
	/** The widget will tick using real time. When not ticking, real time will accumulate and be simulated on the next tick. */
	RealTime,
	/** The widget will tick using game time, respecting pausing and time dilation. */
	GameTime
};

UENUM(BlueprintType)
enum class EWidgetBlendMode : uint8
{
	Opaque,
	Masked,
	Transparent
};

UENUM()
enum class EWidgetGeometryMode : uint8
{
	/** The widget is mapped onto a plane */
	Plane,

	/** The widget is mapped onto a cylinder */
	Cylinder
};


/**
 * The widget component provides a surface in the 3D environment on which to render widgets normally rendered to the screen.
 * Widgets are first rendered to a render target, then that render target is displayed in the world.
 * 
 * Material Properties set by this component on whatever material overrides the default.
 * SlateUI [Texture]
 * BackColor [Vector]
 * TintColorAndOpacity [Vector]
 * OpacityFromTexture [Scalar]
 */
UCLASS(Blueprintable, ClassGroup="UserInterface", hidecategories=(Object,Activation,"Components|Activation",Sockets,Base,Lighting,LOD,Mesh), editinlinenew, meta=(BlueprintSpawnableComponent) )
class UMG_API UWidgetComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** UActorComponent Interface */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/* UPrimitiveComponent Interface */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual UBodySetup* GetBodySetup() override;
	virtual FCollisionShape GetCollisionShape(float Inflation) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	int32 GetNumMaterials() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	void ApplyComponentInstanceData(class FWidgetComponentInstanceData* ComponentInstanceData);
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Ensures the user widget is initialized */
	virtual void InitWidget();

	/** Release resources associated with the widget. */
	virtual void ReleaseResources();

	/** Ensures the 3d window is created its size and content. */
	virtual void UpdateWidget();

	/** Ensure the render target is initialized and updates it if needed. */
	virtual void UpdateRenderTarget(FIntPoint DesiredRenderTargetSize);

	/** 
	* Ensures the body setup is initialized and updates it if needed.
	* @param bDrawSizeChanged Whether the draw size of this component has changed since the last update call.
	*/
	void UpdateBodySetup( bool bDrawSizeChanged = false );

	/**
	 * Converts a world-space hit result to a hit location on the widget
	 * @param HitResult The hit on this widget in the world
	 * @param (Out) The transformed 2D hit location on the widget
	 */
	virtual void GetLocalHitLocation(FVector WorldHitLocation, FVector2D& OutLocalHitLocation) const;

	/**
	 * When using EWidgetGeometryMode::Cylinder, continues the trace from the front face
	 * of the widget component into the cylindrical geometry and returns adjusted hit results information.
	 * 
	 * @returns two hit locations FVector is in world space and a FVector2D is in widget-space.
	 */
	TTuple<FVector, FVector2D> GetCylinderHitLocation(FVector WorldHitLocation, FVector WorldHitDirection) const;

	/** @return Gets the last local location that was hit */
	FVector2D GetLastLocalHitLocation() const
	{
		return LastLocalHitLocation;
	}
	
	/** @return The class of the user widget displayed by this component */
	TSubclassOf<UUserWidget> GetWidgetClass() const { return WidgetClass; }

	/** @return The user widget object displayed by this component */
	UFUNCTION(BlueprintCallable, Category=UserInterface, meta=(UnsafeDuringActorConstruction=true))
	UUserWidget* GetUserWidgetObject() const;

	/** @return Returns the Slate widget that was assigned to this component, if any */
	const TSharedPtr<SWidget>& GetSlateWidget() const;

	/** @return List of widgets with their geometry and the cursor position transformed into this Widget component's space. */
	TArray<FWidgetAndPointer> GetHitWidgetPath(FVector WorldHitLocation, bool bIgnoreEnabledStatus, float CursorRadius = 0.0f);

	/** @return List of widgets with their geometry and the cursor position transformed into this Widget space. The widget space is expressed as a Vector2D. */
	TArray<FWidgetAndPointer> GetHitWidgetPath(FVector2D WidgetSpaceHitCoordinate, bool bIgnoreEnabledStatus, float CursorRadius = 0.0f);

	/** @return The render target to which the user widget is rendered */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UTextureRenderTarget2D* GetRenderTarget() const;

	/** @return The dynamic material instance used to render the user widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UMaterialInstanceDynamic* GetMaterialInstance() const;

	/** @return The window containing the user widget content */
	TSharedPtr<SWindow> GetSlateWindow() const;

	/**  
	 *  Sets the widget to use directly. This function will keep track of the widget till the next time it's called
	 *	with either a newer widget or a nullptr
	 */ 
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	virtual void SetWidget(UUserWidget* Widget);

	/**  
	 *  Sets a Slate widget to be rendered.  You can use this to draw native Slate widgets using a WidgetComponent, instead
	 *  of drawing user widgets.
	 */ 
	virtual void SetSlateWidget( const TSharedPtr<SWidget>& InSlateWidget);

	/**
	 * Sets the local player that owns this widget component.  Setting the owning player controls
	 * which player's viewport the widget appears on in a split screen scenario.  Additionally it
	 * forwards the owning player to the actual UserWidget that is spawned.
	 */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetOwnerPlayer(ULocalPlayer* LocalPlayer);

	/** Gets the local player that owns this widget component. */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	ULocalPlayer* GetOwnerPlayer() const;

	/** @return The draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	FVector2D GetDrawSize() const;

	/** Sets the draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetDrawSize(FVector2D Size);

	/** Requests that the widget be redrawn.  */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	virtual void RequestRedraw();

	/** Gets the blend mode for the widget. */
	EWidgetBlendMode GetBlendMode() const { return BlendMode; }

	/** Sets the blend mode to use for this widget */
	void SetBlendMode( const EWidgetBlendMode NewBlendMode );

	/** Sets whether the widget is two-sided or not */
	void SetTwoSided( const bool bWantTwoSided );

	/** Sets the background color and opacityscale for this widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetBackgroundColor( const FLinearColor NewBackgroundColor );

	/** Sets the tint color and opacity scale for this widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetTintColorAndOpacity( const FLinearColor NewTintColorAndOpacity );

	/** Sets how much opacity from the UI widget's texture alpha is used when rendering to the viewport (0.0-1.0) */
	void SetOpacityFromTexture( const float NewOpacityFromTexture );

	/** @return The pivot point where the UI is rendered about the origin. */
	FVector2D GetPivot() const { return Pivot; }

	/**  */
	void SetPivot( const FVector2D& InPivot ) { Pivot = InPivot; }

	/**  */
	bool GetDrawAtDesiredSize() const { return bDrawAtDesiredSize; }

	/**  */
	void SetDrawAtDesiredSize(bool InbDrawAtDesiredSize) { bDrawAtDesiredSize = InbDrawAtDesiredSize; }

	/**  */
	float GetRedrawTime() const { return RedrawTime; }

	/**  */
	void SetRedrawTime(float bInRedrawTime) { RedrawTime = bInRedrawTime; }

	/** Get the fake window we create for widgets displayed in the world. */
	TSharedPtr< SWindow > GetVirtualWindow() const;
	
	/** Updates the dynamic parameters on the material instance, without re-creating it */
	void UpdateMaterialInstanceParameters();

	/** Sets the widget class used to generate the widget for this component */
	void SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	EWidgetSpace GetWidgetSpace() const { return Space; }

	void SetWidgetSpace( EWidgetSpace NewSpace ) { Space = NewSpace; }

	bool GetEditTimeUsable() const { return bEditTimeUsable; }

	void SetEditTimeUsable(bool Value) { bEditTimeUsable = Value; }

	/** @see EWidgetGeometryMode, @see GetCylinderArcAngle() */
	EWidgetGeometryMode GetGeometryMode() const { return GeometryMode; }

	bool GetReceiveHardwareInput() const { return bReceiveHardwareInput; }

	/** Defines the curvature of the widget component when using EWidgetGeometryMode::Cylinder; ignored otherwise.  */
	float GetCylinderArcAngle() const { return CylinderArcAngle; }

protected:
	/** Just because the user attempts to receive hardware input does not mean it's possible. */
	bool CanReceiveHardwareInput() const;

	void RegisterHitTesterWithViewport(TSharedPtr<SViewport> ViewportWidget);
	void UnregisterHitTesterWithViewport(TSharedPtr<SViewport> ViewportWidget);

	void RegisterWindow();
	void UnregisterWindow();
	void RemoveWidgetFromScreen();

	/** Allows subclasses to control if the widget should be drawn.  Called right before we draw the widget. */
	virtual bool ShouldDrawWidget() const;

	/** Draws the current widget to the render target if possible. */
	virtual void DrawWidgetToRenderTarget(float DeltaTime);

	/** @return the width of the widget component taking GeometryMode into account. */
	float ComputeComponentWidth() const;

protected:
	/** The coordinate space in which to render the widget */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	EWidgetSpace Space;

	/** How this widget should deal with timing, pausing, etc. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	EWidgetTimingPolicy TimingPolicy;

	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	TSubclassOf<UUserWidget> WidgetClass;
	
	/** The size of the displayed quad. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FIntPoint DrawSize;

	/** Should we wait to be told to redraw to actually draw? */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	bool bManuallyRedraw;

	/** Has anyone requested we redraw? */
	UPROPERTY()
	bool bRedrawRequested;

	/**
	 * The time in between draws, if 0 - we would redraw every frame.  If 1, we would redraw every second.
	 * This will work with bManuallyRedraw as well.  So you can say, manually redraw, but only redraw at this
	 * maximum rate.
	 */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	float RedrawTime;

	/** What was the last time we rendered the widget? */
	double LastWidgetRenderTime;

	/** Returns current absolute time, respecting TimingPolicy. */
	double GetCurrentTime() const;

	/**
	 * The actual draw size, this changes based on DrawSize - or the desired size of the widget if
	 * bDrawAtDesiredSize is true.
	 */
	UPROPERTY()
	FIntPoint CurrentDrawSize;

	/**
	 * Causes the render target to automatically match the desired size.
	 * 
	 * WARNING: If you change this every frame, it will be very expensive.  If you need 
	 *    that effect, you should keep the outer widget's sized locked and dynamically
	 *    scale or resize some inner widget.
	 */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	bool bDrawAtDesiredSize;

	/** The Alignment/Pivot point that the widget is placed at relative to the position. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FVector2D Pivot;

	/**
	 * Register with the viewport for hardware input from the true mouse and keyboard.  These widgets
	 * will more or less react like regular 2D widgets in the viewport, e.g. they can and will steal focus
	 * from the viewport.
	 * 
	 * WARNING: If you are making a VR game, definitely do not change this to true.  This option should ONLY be used
	 * if you're making what would otherwise be a normal menu for a game, just in 3D.  If you also need the game to 
	 * remain responsive and for the player to be able to interact with UI and move around the world (such as a keypad on a door), 
	 * use the WidgetInteractionComponent instead.
	 */
	UPROPERTY(EditAnywhere, Category=Interaction)
	bool bReceiveHardwareInput;

	/** Is the virtual window created to host the widget focusable? */
	UPROPERTY(EditAnywhere, Category=Interaction)
	bool bWindowFocusable;

	/**
	 * The owner player for a widget component, if this widget is drawn on the screen, this controls
	 * what player's screen it appears on for split screen, if not set, users player 0.
	 */
	UPROPERTY()
	ULocalPlayer* OwnerPlayer;

	/** The background color of the component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor BackgroundColor;

	/** Tint color and opacity for this component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor TintColorAndOpacity;

	/** Sets the amount of opacity from the widget's UI texture to use when rendering the translucent or masked UI to the viewport (0.0-1.0) */
	UPROPERTY(EditAnywhere, Category=Rendering, meta=(ClampMin=0.0f, ClampMax=1.0f))
	float OpacityFromTexture;

	/** The blend mode for the widget. */
	UPROPERTY(EditAnywhere, Category=Rendering)
	EWidgetBlendMode BlendMode;

	/** Is the component visible from behind? */
	UPROPERTY(EditAnywhere, Category=Rendering)
	bool bIsTwoSided;

	/** Should the component tick the widget when it's off screen? */
	UPROPERTY(EditAnywhere, Category=Animation)
	bool TickWhenOffscreen;

	/** The User Widget object displayed and managed by this component */
	UPROPERTY(Transient, DuplicateTransient)
	UUserWidget* Widget;
	
	/** The Slate widget to be displayed by this component.  Only one of either Widget or SlateWidget can be used */
	TSharedPtr<SWidget> SlateWidget;

	/** The slate widget currently being drawn. */
	TWeakPtr<SWidget> CurrentSlateWidget;

	/** The body setup of the displayed quad */
	UPROPERTY(Transient, DuplicateTransient)
	class UBodySetup* BodySetup;

	/** The material instance for translucent widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial;

	/** The material instance for translucent, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial_OneSided;

	/** The material instance for opaque widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial;

	/** The material instance for opaque, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial_OneSided;

	/** The material instance for masked widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial;

	/** The material instance for masked, one-sided widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial_OneSided;

	/** The target to which the user widget is rendered */
	UPROPERTY(Transient, DuplicateTransient)
	UTextureRenderTarget2D* RenderTarget;

	/** The dynamic instance of the material that the render target is attached to */
	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInstanceDynamic* MaterialInstance;

	UPROPERTY(Transient, DuplicateTransient)
	bool bAddedToScreen;

	/**
	 * Allows the widget component to be used at editor time.  For use in the VR-Editor.
	 */
	UPROPERTY()
	bool bEditTimeUsable;

protected:

	/** Layer Name the widget will live on */
	UPROPERTY(EditDefaultsOnly, Category = Layers)
	FName SharedLayerName;

	/** ZOrder the layer will be created on, note this only matters on the first time a new layer is created, subsequent additions to the same layer will use the initially defined ZOrder */
	UPROPERTY(EditDefaultsOnly, Category = Layers)
	int32 LayerZOrder;

	/** Controls the geometry of the widget component. See EWidgetGeometryMode. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	EWidgetGeometryMode GeometryMode;

	/** Curvature of a cylindrical widget in degrees. */
	UPROPERTY(EditAnywhere, Category=UserInterface, meta=(ClampMin=1.0f, ClampMax=180.0f))
	float CylinderArcAngle;

	/** The slate window that contains the user widget content */
	TSharedPtr<class SVirtualWindow> SlateWindow;

	/** The relative location of the last hit on this component */
	FVector2D LastLocalHitLocation;

	/** The hit tester to use for this component */
	static TSharedPtr<class FWidget3DHitTester> WidgetHitTester;

	/** Helper class for drawing widgets to a render target. */
	TSharedPtr<class FWidgetRenderer> WidgetRenderer;
};
