// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdMode.h"
#include "EditorModeTools.h"
#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "CanvasItem.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "StaticMeshResources.h"
#include "Toolkits/BaseToolkit.h"

#include "CanvasTypes.h"

/** Hit proxy used for editable properties */
struct HPropertyWidgetProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	/** Name of property this is the widget for */
	FString	PropertyName;

	/** If the property is an array property, the index into that array that this widget is for */
	int32	PropertyIndex;

	/** This property is a transform */
	bool	bPropertyIsTransform;

	HPropertyWidgetProxy(FString InPropertyName, int32 InPropertyIndex, bool bInPropertyIsTransform)
		: HHitProxy(HPP_Foreground)
		, PropertyName(InPropertyName)
		, PropertyIndex(InPropertyIndex)
		, bPropertyIsTransform(bInPropertyIsTransform)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

IMPLEMENT_HIT_PROXY(HPropertyWidgetProxy, HHitProxy);


namespace
{
	/**
	 * Returns a reference to the named property value data in the given container.
	 */
	template<typename T>
	T* GetPropertyValuePtrByName(const UStruct* InStruct, void* InContainer, FString PropertyName, int32 ArrayIndex, UProperty*& OutProperty)
	{
		T* ValuePtr = NULL;

		// Extract the vector ptr recursively using the property name
		int32 DelimPos = PropertyName.Find(TEXT("."));
		if(DelimPos != INDEX_NONE)
		{
			// Parse the property name and (optional) array index
			int32 SubArrayIndex = 0;
			FString NameToken = PropertyName.Left(DelimPos);
			int32 ArrayPos = NameToken.Find(TEXT("["));
			if(ArrayPos != INDEX_NONE)
			{
				FString IndexToken = NameToken.RightChop(ArrayPos + 1).LeftChop(1);
				SubArrayIndex = FCString::Atoi(*IndexToken);

				NameToken = PropertyName.Left(ArrayPos);
			}

			// Obtain the property info from the given structure definition
			UProperty* CurrentProp = FindField<UProperty>(InStruct, FName(*NameToken));

			// Check first to see if this is a simple structure (i.e. not an array of structures)
			UStructProperty* StructProp = Cast<UStructProperty>(CurrentProp);
			if(StructProp != NULL)
			{
				// Recursively call back into this function with the structure property and container value
				ValuePtr = GetPropertyValuePtrByName<T>(StructProp->Struct, StructProp->ContainerPtrToValuePtr<void>(InContainer), PropertyName.RightChop(DelimPos + 1), ArrayIndex, OutProperty);
			}
			else
			{
				// Check to see if this is an array
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp);
				if(ArrayProp != NULL)
				{
					// It is an array, now check to see if this is an array of structures
					StructProp = Cast<UStructProperty>(ArrayProp->Inner);
					if(StructProp != NULL)
					{
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
						if(ArrayHelper.IsValidIndex(SubArrayIndex))
						{
							// Recursively call back into this function with the array element and container value
							ValuePtr = GetPropertyValuePtrByName<T>(StructProp->Struct, ArrayHelper.GetRawPtr(SubArrayIndex), PropertyName.RightChop(DelimPos + 1), ArrayIndex, OutProperty);
						}
					}
				}
			}
		}
		else
		{
			UProperty* Prop = FindField<UProperty>(InStruct, FName(*PropertyName));
			if(Prop != NULL)
			{
				if( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop) )
				{
					check(ArrayIndex != INDEX_NONE);

					// Property is an array property, so make sure we have a valid index specified
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
					if( ArrayHelper.IsValidIndex(ArrayIndex) )
					{
						ValuePtr = (T*)ArrayHelper.GetRawPtr(ArrayIndex);
					}
				}
				else
				{
					// Property is a vector property, so access directly
					ValuePtr = Prop->ContainerPtrToValuePtr<T>(InContainer);
				}

				OutProperty = Prop;
			}
		}

		return ValuePtr;
	}

	/**
	 * Returns the value of the property with the given name in the given Actor instance.
	 */
	template<typename T>
	T GetPropertyValueByName(UObject* Object, FString PropertyName, int32 PropertyIndex)
	{
		T Value;
		UProperty* DummyProperty = NULL;
		if (T* ValuePtr = GetPropertyValuePtrByName<T>(Object->GetClass(), Object, PropertyName, PropertyIndex, DummyProperty))
		{
			Value = *ValuePtr;
		}
		return Value;
	}

	/**
	 * Sets the property with the given name in the given Actor instance to the given value.
	 */
	template<typename T>
	void SetPropertyValueByName(UObject* Object, FString PropertyName, int32 PropertyIndex, const T& InValue, UProperty*& OutProperty)
	{
		if (T* ValuePtr = GetPropertyValuePtrByName<T>(Object->GetClass(), Object, PropertyName, PropertyIndex, OutProperty))
		{
			*ValuePtr = InValue;
		}
	}
}

//////////////////////////////////
// FEdMode

void FEdMode::FPropertyWidgetInfo::GetTransformAndColor(UObject* BestSelectedItem, bool bIsSelected, FTransform& OutLocalTransform, FString& OutValidationMessage, FColor& OutDrawColor) const
{
	// Determine the desired position
	if (bIsTransform)
	{
		OutLocalTransform = GetPropertyValueByName<FTransform>(BestSelectedItem, PropertyName, PropertyIndex);
	}
	else
	{
		OutLocalTransform = FTransform(GetPropertyValueByName<FVector>(BestSelectedItem, PropertyName, PropertyIndex));
	}

	// Determine the desired color
	OutDrawColor = bIsSelected ? FColor::White : FColor(128, 128, 255);
	if (PropertyValidationName != NAME_None)
	{
		if (UFunction* ValidateFunc = BestSelectedItem->FindFunction(PropertyValidationName))
		{
			BestSelectedItem->ProcessEvent(ValidateFunc, &OutValidationMessage);

			// if we have a negative result, the widget color is red.
			OutDrawColor = OutValidationMessage.IsEmpty() ? OutDrawColor : FColor::Red;
		}
	}
}

//////////////////////////////////
// FEdMode

const FName FEdMode::MD_MakeEditWidget(TEXT("MakeEditWidget"));
const FName FEdMode::MD_ValidateWidgetUsing(TEXT("ValidateWidgetUsing"));

FEdMode::FEdMode()
	: bPendingDeletion(false)
	, CurrentWidgetAxis(EAxisList::None)
	, CurrentTool(nullptr)
	, Owner(nullptr)
	, EditedPropertyIndex(INDEX_NONE)
	, bEditedPropertyIsTransform(false)
{
	bDrawKillZ = true;
}

FEdMode::~FEdMode()
{
}

void FEdMode::OnModeUnregistered( FEditorModeID ModeID )
{
	if( ModeID == Info.ID )
	{
		// This should be synonymous with "delete this"
		Owner->DestroyMode(ModeID);
	}
}

bool FEdMode::MouseEnter( FEditorViewportClient* ViewportClient,FViewport* Viewport,int32 x, int32 y )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseEnter( ViewportClient, Viewport, x, y );
	}

	return false;
}

bool FEdMode::MouseLeave( FEditorViewportClient* ViewportClient,FViewport* Viewport )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseLeave( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::MouseMove(FEditorViewportClient* ViewportClient,FViewport* Viewport,int32 x, int32 y)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseMove( ViewportClient, Viewport, x, y );
	}

	return false;
}

bool FEdMode::ReceivedFocus(FEditorViewportClient* ViewportClient,FViewport* Viewport)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->ReceivedFocus( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::LostFocus(FEditorViewportClient* ViewportClient,FViewport* Viewport)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->LostFocus( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->CapturedMouseMove( InViewportClient, InViewport, InMouseX, InMouseY );
	}

	return false;
}

bool FEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	// First try the currently selected tool
	if ((GetCurrentTool() != nullptr) && GetCurrentTool()->InputKey(ViewportClient, Viewport, Key, Event))
	{
		return true;
	}
	else
	{
		// Next pass input to the mode toolkit
		if (Toolkit.IsValid() && ((Event == IE_Pressed) || (Event == IE_Repeat)))
		{
			if (Toolkit->GetToolkitCommands()->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), (Event == IE_Repeat)))
			{
				return true;
			}
		}

		// Finally, pass input up to selected actors if not in a tool mode
		TArray<AActor*> SelectedActors;
		Owner->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

		for (TArray<AActor*>::TIterator It(SelectedActors); It; ++It)
		{
			// Tell the object we've had a key press
			(*It)->EditorKeyPressed(Key, Event);
		}
	}

	return false;
}

bool FEdMode::InputAxis(FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime)
{
	FModeTool* Tool = GetCurrentTool();
	if (Tool)
	{
		return Tool->InputAxis(InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
	}

	return false;
}

bool FEdMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{	
	if (UsesPropertyWidgets())
	{
		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);

		if ((BestSelectedItem != nullptr) && (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None))
		{
			GEditor->NoteActorMovement();

			if (!EditedPropertyName.IsEmpty())
			{
				FTransform LocalTM = FTransform::Identity;

				if (bEditedPropertyIsTransform)
				{
					LocalTM = GetPropertyValueByName<FTransform>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex);
				}
				else
				{					
					FVector LocalPos = GetPropertyValueByName<FVector>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex);
					LocalTM = FTransform(LocalPos);
				}

				// Calculate world transform
				FTransform WorldTM = LocalTM * DisplayWidgetToWorld;
				// Calc delta specified by drag
				//FTransform DeltaTM(InRot.Quaternion(), InDrag);
				// Apply delta in world space
				WorldTM.SetTranslation(WorldTM.GetTranslation() + InDrag);
				WorldTM.SetRotation(InRot.Quaternion() * WorldTM.GetRotation());
				// Convert new world transform back into local space
				LocalTM = WorldTM.GetRelativeTransform(DisplayWidgetToWorld);
				// Apply delta scale
				LocalTM.SetScale3D(LocalTM.GetScale3D() + InScale);

				BestSelectedItem->PreEditChange(NULL);

				// Property that we actually change
				UProperty* SetProperty = NULL;

				if (bEditedPropertyIsTransform)
				{
					SetPropertyValueByName<FTransform>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex, LocalTM, SetProperty);
				}
				else
				{
					SetPropertyValueByName<FVector>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex, LocalTM.GetLocation(), SetProperty);
				}

				FPropertyChangedEvent PropertyChangeEvent(SetProperty);
				BestSelectedItem->PostEditChangeProperty(PropertyChangeEvent);

				return true;
			}
		}
	}

	if (GetCurrentTool())
	{
		return GetCurrentTool()->InputDelta(InViewportClient,InViewport,InDrag,InRot,InScale);
	}

	return false;
}

bool FEdMode::UsesTransformWidget() const
{
	if (GetCurrentTool())
	{
		return GetCurrentTool()->UseWidget();
	}

	return true;
}

bool FEdMode::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	if (UsesPropertyWidgets())
	{
		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);

		if (BestSelectedItem != nullptr)
		{
			// If editing a vector (not a transform)
			if (!EditedPropertyName.IsEmpty() && !bEditedPropertyIsTransform)
			{
				return (CheckMode == FWidget::WM_Translate);
			}
		}
	}

	return true;
}

FVector FEdMode::GetWidgetLocation() const
{
	if (UsesPropertyWidgets())
	{
		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);
		if (BestSelectedItem)
		{
			if (!EditedPropertyName.IsEmpty())
			{
				FVector LocalPos = FVector::ZeroVector;

				if (bEditedPropertyIsTransform)
				{
					FTransform LocalTM = GetPropertyValueByName<FTransform>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex);
					LocalPos = LocalTM.GetLocation();
				}
				else
				{
					LocalPos = GetPropertyValueByName<FVector>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex);
				}

				const FVector WorldPos = DisplayWidgetToWorld.TransformPosition(LocalPos);
				return WorldPos;
			}
		}
	}

	return Owner->PivotLocation;
}

bool FEdMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (UsesPropertyWidgets())
	{
		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);
		if (BestSelectedItem)
		{
			if (EditedPropertyName != TEXT(""))
			{
				if (bEditedPropertyIsTransform)
				{
					FTransform LocalTM = GetPropertyValueByName<FTransform>(BestSelectedItem, EditedPropertyName, EditedPropertyIndex);
					InMatrix = FRotationMatrix::Make((LocalTM * DisplayWidgetToWorld).GetRotation());
					return true;
				}
				else
				{
					InMatrix = FRotationMatrix::Make(DisplayWidgetToWorld.GetRotation());
					return true;
				}
			}
		}
	}

	return false;
}

bool FEdMode::ShouldDrawWidget() const
{
	bool bDrawWidget = false;

	bool bHadSelectableComponents = false;
	if (Owner->GetSelectedComponents()->Num() > 0)
	{
		// when components are selected, only show the widget when one or more are scene components
		for (FSelectedEditableComponentIterator It(*Owner->GetSelectedComponents()); It; ++It)
		{
			bHadSelectableComponents = true;
			if (It->IsA<USceneComponent>())
			{
				bDrawWidget = true;
				break;
			}
		}
	}

	if (!bHadSelectableComponents)
	{
		// when actors are selected, only show the widget when all selected actors have scene components
		bDrawWidget = Owner->SelectionHasSceneComponent();
	}

	return bDrawWidget;
}

EAxisList::Type FEdMode::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	return EAxisList::All;
}

bool FEdMode::BoxSelect( FBox& InBox, bool InSelect )
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->BoxSelect( InBox, InSelect );
	}
	return bResult;
}

bool FEdMode::FrustumSelect( const FConvexVolume& InFrustum, bool InSelect )
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->FrustumSelect( InFrustum, InSelect );
	}
	return bResult;
}

void FEdMode::SelectNone()
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->SelectNone();
	}
}

void FEdMode::Tick(FEditorViewportClient* ViewportClient,float DeltaTime)
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->Tick(ViewportClient,DeltaTime);
	}
}

void FEdMode::ActorSelectionChangeNotify()
{
	EditedPropertyName = TEXT("");
	EditedPropertyIndex = INDEX_NONE;
	bEditedPropertyIsTransform = false;
}

bool FEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (UsesPropertyWidgets() && (HitProxy != nullptr))
	{
		if( HitProxy->IsA(HPropertyWidgetProxy::StaticGetType()) )
		{
			HPropertyWidgetProxy* PropertyProxy = (HPropertyWidgetProxy*)HitProxy;
			EditedPropertyName = PropertyProxy->PropertyName;
			EditedPropertyIndex = PropertyProxy->PropertyIndex;
			bEditedPropertyIsTransform = PropertyProxy->bPropertyIsTransform;
			return true;
		}
		// Left clicking on an actor, stop editing a property
		else if( HitProxy->IsA(HActor::StaticGetType()) )
		{
			EditedPropertyName = FString();
			EditedPropertyIndex = INDEX_NONE;
			bEditedPropertyIsTransform = false;
		}
	}

	return false;
}

void FEdMode::Enter()
{
	// Update components for selected actors, in case the mode we just exited
	// was hijacking selection events selection and not updating components.
	for ( FSelectionIterator It( *Owner->GetSelectedActors() ) ; It ; ++It )
	{
		AActor* SelectedActor = CastChecked<AActor>( *It );
		SelectedActor->MarkComponentsRenderStateDirty();
	}

	bPendingDeletion = false;

	FEditorDelegates::EditorModeEnter.Broadcast( this );
	const bool bIsEnteringMode = true;
	Owner->BroadcastEditorModeChanged( this, bIsEnteringMode );
}

void FEdMode::Exit()
{
	const bool bIsEnteringMode = false;
	Owner->BroadcastEditorModeChanged( this, bIsEnteringMode );
	FEditorDelegates::EditorModeExit.Broadcast( this );
}

void FEdMode::SetCurrentTool( EModeTools InID )
{
	CurrentTool = FindTool( InID );
	check( CurrentTool );	// Tool not found!  This can't happen.

	CurrentToolChanged();
}

void FEdMode::SetCurrentTool( FModeTool* InModeTool )
{
	CurrentTool = InModeTool;
	check(CurrentTool);

	CurrentToolChanged();
}

FModeTool* FEdMode::FindTool( EModeTools InID )
{
	for( int32 x = 0 ; x < Tools.Num() ; ++x )
	{
		if( Tools[x]->GetID() == InID )
		{
			return Tools[x];
		}
	}

	UE_LOG(LogEditorModes, Fatal, TEXT("FEdMode::FindTool failed to find tool %d"), (int32)InID);
	return NULL;
}

void FEdMode::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	// Let the current mode tool render if it wants to
	if (FModeTool* Tool = GetCurrentTool())
	{
		Tool->Render( View, Viewport, PDI );
	}

	if (UsesPropertyWidgets())
	{
		const bool bHitTesting = PDI->IsHitTesting();

		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);
		
		if (BestSelectedItem != nullptr)
		{
			UClass* Class = BestSelectedItem->GetClass();
			TArray<FPropertyWidgetInfo> WidgetInfos;
			GetPropertyWidgetInfos(Class, BestSelectedItem, WidgetInfos);
			FEditorScriptExecutionGuard ScriptGuard;
			for (const FPropertyWidgetInfo& WidgetInfo : WidgetInfos)
			{
				const bool bSelected = (WidgetInfo.PropertyName == EditedPropertyName) && (WidgetInfo.PropertyIndex == EditedPropertyIndex);

				FTransform LocalWidgetTransform;
				FString ValidationMessage;
				FColor WidgetColor;
				WidgetInfo.GetTransformAndColor(BestSelectedItem, bSelected, /*out*/ LocalWidgetTransform, /*out*/ ValidationMessage, /*out*/ WidgetColor);

				const FTransform WorldWidgetTransform = LocalWidgetTransform * DisplayWidgetToWorld;
				const FMatrix WidgetTM(WorldWidgetTransform.ToMatrixWithScale());

				const float WidgetSize = 0.035f;
				const float ZoomFactor = FMath::Min<float>(View->ViewMatrices.GetProjectionMatrix().M[0][0], View->ViewMatrices.GetProjectionMatrix().M[1][1]);
				const float WidgetRadius = View->Project(WorldWidgetTransform.GetTranslation()).W * (WidgetSize / ZoomFactor);

				if (bHitTesting) PDI->SetHitProxy(new HPropertyWidgetProxy(WidgetInfo.PropertyName, WidgetInfo.PropertyIndex, WidgetInfo.bIsTransform));
				DrawWireDiamond(PDI, WidgetTM, WidgetRadius, WidgetColor, SDPG_Foreground );
				if(bHitTesting) PDI->SetHitProxy( NULL );
			}
		}
	}
}

void FEdMode::DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	// Render the drag tool.
	ViewportClient->RenderDragTool( View, Canvas );

	// Let the current mode tool draw a HUD if it wants to
	FModeTool* tool = GetCurrentTool();
	if( tool )
	{
		tool->DrawHUD( ViewportClient, Viewport, View, Canvas );
	}

	if (ViewportClient->IsPerspective() && GetDefault<ULevelEditorViewportSettings>()->bHighlightWithBrackets)
	{
		DrawBrackets( ViewportClient, Viewport, View, Canvas );
	}

	// If this viewport doesn't show mode widgets or the mode itself doesn't want them, leave.
	if( !(ViewportClient->EngineShowFlags.ModeWidgets) || !ShowModeWidgets() )
	{
		return;
	}

	// Clear Hit proxies
	const bool bIsHitTesting = Canvas->IsHitTesting();
	if ( !bIsHitTesting )
	{
		Canvas->SetHitProxy(NULL);
	}

	// Draw vertices for selected BSP brushes and static meshes if the large vertices show flag is set.
	if ( !ViewportClient->bDrawVertices )
	{
		return;
	}

	const bool bLargeVertices		= View->Family->EngineShowFlags.LargeVertices;
	const bool bShowBrushes			= View->Family->EngineShowFlags.Brushes;
	const bool bShowBSP				= View->Family->EngineShowFlags.BSP;
	const bool bShowBuilderBrush	= View->Family->EngineShowFlags.BuilderBrush != 0;

	UTexture2D* VertexTexture = GetVertexTexture();
	const float TextureSizeX		= VertexTexture->GetSizeX() * ( bLargeVertices ? 1.0f : 0.5f );
	const float TextureSizeY		= VertexTexture->GetSizeY() * ( bLargeVertices ? 1.0f : 0.5f );

	// Temporaries.
	TArray<FVector> Vertices;

	for ( FSelectionIterator It( *Owner->GetSelectedActors() ) ; It ; ++It )
	{
		AActor* SelectedActor = static_cast<AActor*>( *It );
		checkSlow( SelectedActor->IsA(AActor::StaticClass()) );

		if( bLargeVertices )
		{
			FCanvasItemTestbed::bTestState = !FCanvasItemTestbed::bTestState;

			// Static mesh vertices
			AStaticMeshActor* Actor = Cast<AStaticMeshActor>( SelectedActor );
			if( Actor && Actor->GetStaticMeshComponent() && Actor->GetStaticMeshComponent()->GetStaticMesh()
				&& Actor->GetStaticMeshComponent()->GetStaticMesh()->RenderData )
			{
				FTransform ActorToWorld = Actor->ActorToWorld();
				Vertices.Empty();
				const FPositionVertexBuffer& VertexBuffer = Actor->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].PositionVertexBuffer;
				for( uint32 i = 0 ; i < VertexBuffer.GetNumVertices() ; i++ )
				{
					Vertices.AddUnique( ActorToWorld.TransformPosition( VertexBuffer.VertexPosition(i) ) );
				}

				FCanvasTileItem TileItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FLinearColor::White );
				TileItem.BlendMode = SE_BLEND_Translucent;
				for( int32 VertexIndex = 0 ; VertexIndex < Vertices.Num() ; ++VertexIndex )
				{				
					const FVector& Vertex = Vertices[VertexIndex];
					FVector2D PixelLocation;
					if(View->ScreenToPixel(View->WorldToScreen(Vertex),PixelLocation))
					{
						const bool bOutside =
							PixelLocation.X < 0.0f || PixelLocation.X > View->ViewRect.Width() ||
							PixelLocation.Y < 0.0f || PixelLocation.Y > View->ViewRect.Height();
						if( !bOutside )
						{
							const float X = PixelLocation.X - (TextureSizeX/2);
							const float Y = PixelLocation.Y - (TextureSizeY/2);
							if( bIsHitTesting ) 
							{
								Canvas->SetHitProxy( new HStaticMeshVert(Actor,Vertex) );
							}
							TileItem.Texture = VertexTexture->Resource;
							
							TileItem.Size = FVector2D( TextureSizeX, TextureSizeY );
							Canvas->DrawItem( TileItem, FVector2D( X, Y ) );							
							if( bIsHitTesting )
							{
								Canvas->SetHitProxy( NULL );
							}
						}
					}
				}
			}
		}
	}

	if (UsesPropertyWidgets())
	{
		FTransform DisplayWidgetToWorld;
		UObject* BestSelectedItem = GetItemToTryDisplayingWidgetsFor(/*out*/ DisplayWidgetToWorld);
		if (BestSelectedItem != nullptr)
		{
			FEditorScriptExecutionGuard ScriptGuard;

			const int32 HalfX = 0.5f * Viewport->GetSizeXY().X;
			const int32 HalfY = 0.5f * Viewport->GetSizeXY().Y;

			UClass* Class = BestSelectedItem->GetClass();		
			TArray<FPropertyWidgetInfo> WidgetInfos;
			GetPropertyWidgetInfos(Class, BestSelectedItem, WidgetInfos);
			for (const FPropertyWidgetInfo& WidgetInfo : WidgetInfos)
			{
				FTransform LocalWidgetTransform;
				FString ValidationMessage;
				FColor IgnoredWidgetColor;
				WidgetInfo.GetTransformAndColor(BestSelectedItem, /*bSelected=*/ false, /*out*/ LocalWidgetTransform, /*out*/ ValidationMessage, /*out*/ IgnoredWidgetColor);

				const FTransform WorldWidgetTransform = LocalWidgetTransform * DisplayWidgetToWorld;

				const FPlane Proj = View->Project(WorldWidgetTransform.GetTranslation());
				if (Proj.W > 0.f)
				{
					// do some string fixing
					const uint32 VectorIndex = WidgetInfo.PropertyIndex;
					const FString WidgetDisplayName = WidgetInfo.DisplayName + ((VectorIndex != INDEX_NONE) ? FString::Printf(TEXT("[%d]"), VectorIndex) : TEXT(""));
					const FString DisplayString = ValidationMessage.IsEmpty() ? WidgetDisplayName : ValidationMessage;

					const int32 XPos = HalfX + ( HalfX * Proj.X );
					const int32 YPos = HalfY + ( HalfY * (Proj.Y * -1.f) );
					FCanvasTextItem TextItem( FVector2D( XPos + 5, YPos), FText::FromString( DisplayString ), GEngine->GetSmallFont(), FLinearColor::White );
					TextItem.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem( TextItem );
				}
			}
		}
	}
}

void FEdMode::DrawBrackets( FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast<AActor>( SelectedActors.GetSelectedObject(CurSelectedActorIndex ) );
		if( SelectedActor != NULL )
		{
			// Draw a bracket for selected "paintable" static mesh actors
			const bool bIsValidActor = ( Cast< AStaticMeshActor >( SelectedActor ) != NULL );

			const FLinearColor SelectedActorBoxColor( 0.6f, 0.6f, 1.0f );
			const bool bDrawBracket = bIsValidActor;
			ViewportClient->DrawActorScreenSpaceBoundingBox( Canvas, View, Viewport, SelectedActor, SelectedActorBoxColor, bDrawBracket );
		}
	}
}

bool FEdMode::UsesToolkits() const
{
	return false;
}

UWorld* FEdMode::GetWorld() const
{
	return Owner->GetWorld();
}

class FEditorModeTools* FEdMode::GetModeManager() const
{
	return Owner;
}

bool FEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->StartModify();
	}
	return bResult;
}

bool FEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->EndModify();
	}
	return bResult;
}

FVector FEdMode::GetWidgetNormalFromCurrentAxis( void* InData )
{
	// Figure out the proper coordinate system.

	FMatrix matrix = FMatrix::Identity;
	if( Owner->GetCoordSystem() == COORD_Local )
	{
		GetCustomDrawingCoordinateSystem( matrix, InData );
	}

	// Get a base normal from the current axis.

	FVector BaseNormal(1,0,0);		// Default to X axis
	switch( CurrentWidgetAxis )
	{
		case EAxisList::Y:	BaseNormal = FVector(0,1,0);	break;
		case EAxisList::Z:	BaseNormal = FVector(0,0,1);	break;
		case EAxisList::XY:	BaseNormal = FVector(1,1,0);	break;
		case EAxisList::XZ:	BaseNormal = FVector(1,0,1);	break;
		case EAxisList::YZ:	BaseNormal = FVector(0,1,1);	break;
		case EAxisList::XYZ:	BaseNormal = FVector(1,1,1);	break;
	}

	// Transform the base normal into the proper coordinate space.
	return matrix.TransformPosition( BaseNormal );
}

AActor* FEdMode::GetFirstSelectedActorInstance() const
{
	return Owner->GetSelectedActors()->GetTop<AActor>();
}

bool FEdMode::CanCreateWidgetForStructure(const UStruct* InPropStruct)
{
	return InPropStruct && (InPropStruct->GetFName() == NAME_Vector || InPropStruct->GetFName() == NAME_Transform);
}

bool FEdMode::CanCreateWidgetForProperty(UProperty* InProp)
{
	UStructProperty* TestProperty = Cast<UStructProperty>(InProp);
	if( !TestProperty )
	{
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(InProp);
		if( ArrayProperty )
		{
			TestProperty = Cast<UStructProperty>(ArrayProperty->Inner);
		}
	}
	return (TestProperty != NULL) && CanCreateWidgetForStructure(TestProperty->Struct);
}

bool FEdMode::ShouldCreateWidgetForProperty(UProperty* InProp)
{
	return CanCreateWidgetForProperty(InProp) && InProp->HasMetaData(MD_MakeEditWidget);
}

static bool IsTransformProperty(UProperty* InProp)
{
	UStructProperty* StructProp = Cast<UStructProperty>(InProp);
	return (StructProp != NULL && StructProp->Struct->GetFName() == NAME_Transform);

}

struct FPropertyWidgetInfoChainElement
{
	UProperty* Property;
	int32 Index;

	FPropertyWidgetInfoChainElement(UProperty* InProperty = nullptr, int32 InIndex = INDEX_NONE)
		: Property(InProperty), Index(InIndex)
	{}

	static bool ShouldCreateWidgetSomwhereInBranch(UProperty* InProp)
	{
		UStructProperty* StructProperty = Cast<UStructProperty>(InProp);
		if (!StructProperty)
		{
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(InProp);
			if (ArrayProperty)
			{
				StructProperty = Cast<UStructProperty>(ArrayProperty->Inner);
			}
		}

		if (StructProperty)
		{
			if (FEdMode::CanCreateWidgetForStructure(StructProperty->Struct) && InProp->HasMetaData(FEdMode::MD_MakeEditWidget))
			{
				return true;
			}

			for (TFieldIterator<UProperty> PropertyIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				if (ShouldCreateWidgetSomwhereInBranch(*PropertyIt))
				{
					return true;
				}
			}
		}

		return false;
	}

	static FEdMode::FPropertyWidgetInfo CreateWidgetInfo(const TArray<FPropertyWidgetInfoChainElement>& Chain, bool bIsTransform, UProperty* CurrentProp, int32 Index = INDEX_NONE)
	{
		check(CurrentProp);
		FEdMode::FPropertyWidgetInfo WidgetInfo;
		WidgetInfo.PropertyValidationName = FName(*CurrentProp->GetMetaData(FEdMode::MD_ValidateWidgetUsing));
		WidgetInfo.bIsTransform = bIsTransform;
		WidgetInfo.PropertyIndex = Index;

		const FString SimplePostFix(TEXT("."));
		for (int32 ChainIndex = 0; ChainIndex < Chain.Num(); ++ChainIndex)
		{
			const FPropertyWidgetInfoChainElement& Element = Chain[ChainIndex];
			check(Element.Property);
			const FString Postfix = (Element.Index != INDEX_NONE) ? FString::Printf(TEXT("[%d]."), Element.Index) : SimplePostFix;
			const FString PropertyName = Element.Property->GetName() + Postfix;
			const FString& DisplayName = Element.Property->GetMetaData(TEXT("DisplayName"));

			WidgetInfo.PropertyName += PropertyName;
			WidgetInfo.DisplayName += (!DisplayName.IsEmpty()) ? (DisplayName + Postfix) : PropertyName;
		}

		{
			const FString PropertyName = CurrentProp->GetName();
			const FString& DisplayName = CurrentProp->GetMetaData(TEXT("DisplayName"));

			WidgetInfo.PropertyName += PropertyName;
			WidgetInfo.DisplayName += (!DisplayName.IsEmpty()) ? DisplayName : PropertyName;
		}
		return WidgetInfo;
	}

	static void RecursiveGet(const FEdMode& EdMode, const UStruct* InStruct, const void* InContainer, TArray<FEdMode::FPropertyWidgetInfo>& OutInfos, TArray<FPropertyWidgetInfoChainElement>& Chain)
	{
		for (TFieldIterator<UProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* CurrentProp = *PropertyIt;
			check(CurrentProp);

			if (EdMode.ShouldCreateWidgetForProperty(CurrentProp))
			{
				if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp))
				{
					check(InContainer);
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
					// See how many widgets we need to make for the array property
					const uint32 ArrayDim = ArrayHelper.Num();
					for (uint32 Index = 0; Index < ArrayDim; ++Index)
					{
						OutInfos.Add(FPropertyWidgetInfoChainElement::CreateWidgetInfo(Chain, IsTransformProperty(ArrayProp->Inner), CurrentProp, Index));
					}
				}
				else
				{
					OutInfos.Add(FPropertyWidgetInfoChainElement::CreateWidgetInfo(Chain, IsTransformProperty(CurrentProp), CurrentProp));
				}
			}
			else if (UStructProperty* StructProp = Cast<UStructProperty>(CurrentProp))
			{
				// Recursively traverse into structures, looking for additional vector properties to expose
				Chain.Push(FPropertyWidgetInfoChainElement(StructProp));
				RecursiveGet(EdMode
					, StructProp->Struct
					, StructProp->ContainerPtrToValuePtr<void>(InContainer)
					, OutInfos
					, Chain);
				Chain.Pop(false);
			}
			else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp))
			{
				// Recursively traverse into arrays of structures, looking for additional vector properties to expose
				UStructProperty* InnerStructProp = Cast<UStructProperty>(ArrayProp->Inner);
				if (InnerStructProp)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);

					// If the array is not empty the do additional check to tell if iteration is necessary
					if (ArrayHelper.Num() && ShouldCreateWidgetSomwhereInBranch(InnerStructProp))
					{
						for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
						{
							Chain.Push(FPropertyWidgetInfoChainElement(ArrayProp, ArrayIndex));
							RecursiveGet(EdMode
								, InnerStructProp->Struct
								, ArrayHelper.GetRawPtr(ArrayIndex)
								, OutInfos
								, Chain);
							Chain.Pop(false);
						}
					}
				}
			}
		}
	}
};

UObject* FEdMode::GetItemToTryDisplayingWidgetsFor(FTransform& OutLocalToWorld) const
{
	// Determine what is selected, preferring a component over an actor
	USceneComponent* SelectedComponent = Owner->GetSelectedComponents()->GetTop<USceneComponent>();
	UObject* BestSelectedItem = SelectedComponent;

	if (SelectedComponent == nullptr)
	{
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if (SelectedActor != nullptr)
		{
			if (USceneComponent* RootComponent = SelectedActor->GetRootComponent())
			{
				BestSelectedItem = SelectedActor;
				OutLocalToWorld = RootComponent->GetComponentToWorld();
			}
		}
	}
	else
	{
		OutLocalToWorld = SelectedComponent->GetComponentToWorld();
	}

	return BestSelectedItem;
}

void FEdMode::GetPropertyWidgetInfos(const UStruct* InStruct, const void* InContainer, TArray<FPropertyWidgetInfo>& OutInfos) const
{
	TArray<FPropertyWidgetInfoChainElement> Chain;
	FPropertyWidgetInfoChainElement::RecursiveGet(*this, InStruct, InContainer, OutInfos, Chain);
}

bool FEdMode::IsSnapRotationEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled;
}
