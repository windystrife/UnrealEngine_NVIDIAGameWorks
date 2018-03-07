// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SEditorViewportViewMenu.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "EditorViewportCommands.h"

#define LOCTEXT_NAMESPACE "EditorViewportViewMenu"

void SEditorViewportViewMenu::Construct( const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar )
{
	Viewport = InViewport;
	MenuExtenders = InArgs._MenuExtenders;

	SEditorViewportToolbarMenu::Construct
	(
		SEditorViewportToolbarMenu::FArguments()
			.ParentToolBar( InParentToolBar)
			.Cursor( EMouseCursor::Default )
			.Label(this, &SEditorViewportViewMenu::GetViewMenuLabel)
			.LabelIcon(this, &SEditorViewportViewMenu::GetViewMenuLabelIcon)
			.OnGetMenuContent( this, &SEditorViewportViewMenu::GenerateViewMenuContent )
	);
		
}

FText SEditorViewportViewMenu::GetViewMenuLabel() const
{
	FText Label = LOCTEXT("ViewMenuTitle_Default", "View");
	TSharedPtr< SEditorViewport > PinnedViewport = Viewport.Pin();
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetViewportClient()->GetViewMode() )
		{
			case VMI_BrushWireframe:
				Label = LOCTEXT("ViewMenuTitle_BrushWireframe", "Wireframe");
				break;

			case VMI_Wireframe:
				Label = LOCTEXT("ViewMenuTitle_Wireframe", "Wireframe");
				break;

			case VMI_Unlit:
				Label = LOCTEXT("ViewMenuTitle_Unlit", "Unlit");
				break;

			case VMI_Lit:
				Label = LOCTEXT("ViewMenuTitle_Lit", "Lit");
				break;

			case VMI_Lit_DetailLighting:
				Label = LOCTEXT("ViewMenuTitle_DetailLighting", "Detail Lighting");
				break;

			case VMI_LightingOnly:
				Label = LOCTEXT("ViewMenuTitle_LightingOnly", "Lighting Only");
				break;

			case VMI_LightComplexity:
				Label = LOCTEXT("ViewMenuTitle_LightComplexity", "Light Complexity");
				break;

			case VMI_ShaderComplexity:
				Label = LOCTEXT("ViewMenuTitle_ShaderComplexity", "Shader Complexity");
				break;

			case VMI_QuadOverdraw:
				Label = LOCTEXT("ViewMenuTitle_QuadOverdraw", "Quad Overdraw");
				break;

			case VMI_ShaderComplexityWithQuadOverdraw:
				Label = LOCTEXT("ViewMenuTitle_ShaderComplexityWithQuadOverdraw", "Shader Complexity & Quads");
				break;

			case VMI_PrimitiveDistanceAccuracy:
				Label = LOCTEXT("ViewMenuTitle_PrimitiveDistanceAccuracy", "Primitive Distance Accuracy");
				break;

			case VMI_MeshUVDensityAccuracy:
				Label = LOCTEXT("ViewMenuTitle_MeshUVDensityAccuracy", "Mesh UV Densities Accuracy");
				break;

			case VMI_MaterialTextureScaleAccuracy:
				Label = LOCTEXT("ViewMenuTitle_MaterialTextureScaleAccuracy", "Material Texture Scales Accuracy");
				break;

			case VMI_RequiredTextureResolution:
				Label = LOCTEXT("ViewMenuTitle_Required Texture Resolution", "Required Texture Resolution");
				break;

			case VMI_StationaryLightOverlap:
				Label = LOCTEXT("ViewMenuTitle_StationaryLightOverlap", "Stationary Light Overlap");
				break;

			case VMI_LightmapDensity:
				Label = LOCTEXT("ViewMenuTitle_LightmapDensity", "Lightmap Density");
				break;

			case VMI_ReflectionOverride:
				Label = LOCTEXT("ViewMenuTitle_ReflectionOverride", "Reflections");
				break;

			case VMI_VisualizeBuffer:
				Label = LOCTEXT("ViewMenuTitle_VisualizeBuffer", "Buffer Visualization");
				break;

			case VMI_CollisionPawn:
				Label = LOCTEXT("ViewMenuTitle_CollisionPawn", "Player Collision");
				break;

			case VMI_CollisionVisibility:
				Label = LOCTEXT("ViewMenuTitle_CollisionVisibility", "Visibility Collision");
				break;

			case VMI_LitLightmapDensity:
				break;

			case VMI_LODColoration:
				Label = LOCTEXT("ViewMenuTitle_LODColoration", "LOD Coloration");
				break;
			case VMI_HLODColoration:
				Label = LOCTEXT("ViewMenuTitle_HLODColoration", "HLOD Coloration");
				break;
		}
	}

	return Label;
}

const FSlateBrush* SEditorViewportViewMenu::GetViewMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SEditorViewport > PinnedViewport = Viewport.Pin();
	if( PinnedViewport.IsValid() )
	{
		static FName WireframeIcon( "EditorViewport.WireframeMode" );
		static FName UnlitIcon( "EditorViewport.UnlitMode" );
		static FName LitIcon( "EditorViewport.LitMode" );
		static FName DetailLightingIcon("EditorViewport.DetailLightingMode");
		static FName LightingOnlyIcon("EditorViewport.LightingOnlyMode");
		static FName LightComplexityIcon("EditorViewport.LightComplexityMode");
		static FName ShaderComplexityIcon("EditorViewport.ShaderComplexityMode");
		static FName QuadOverdrawIcon("EditorViewport.QuadOverdrawMode");
		static FName ShaderComplexityWithQuadOverdrawIcon("EditorViewport.ShaderCOmplexityWithQuadOverdrawMode");
		static FName PrimitiveDistanceAccuracyIcon("EditorViewport.TexStreamAccPrimitiveDistanceMode");
		static FName MeshUVDensityAccuracyIcon("EditorViewport.TexStreamAccMeshUVDensityMode");
		static FName MaterialTextureScaleAccuracyIcon("EditorViewport.TexStreamAccMaterialTextureScaleMode");
		static FName RequiredTextureResolutionIcon("EditorViewport.RequiredTextureResolutionMode");
		static FName LightOverlapIcon("EditorViewport.StationaryLightOverlapMode");
		static FName LightmapDensityIcon("EditorViewport.LightmapDensityMode");
		static FName ReflectionModeIcon("EditorViewport.ReflectionOverrideMode");
		static FName LODColorationIcon("EditorViewport.LODColorationMode");
		static FName VisualizeBufferIcon("EditorViewport.VisualizeBufferMode");
		static FName CollisionPawnIcon("EditorViewport.CollisionPawn");
		static FName CollisionVisibilityIcon("EditorViewport.CollisionVisibility");

		switch( PinnedViewport->GetViewportClient()->GetViewMode() )
		{
			case VMI_BrushWireframe:
				Icon = WireframeIcon;
				break;

			case VMI_Wireframe:
				Icon = WireframeIcon;
				break;

			case VMI_Unlit:
				Icon = UnlitIcon;
				break;

			case VMI_Lit:
				Icon = LitIcon;
				break;

			case VMI_Lit_DetailLighting:
				Icon = DetailLightingIcon;
				break;

			case VMI_LightingOnly:
				Icon = LightingOnlyIcon;
				break;

			case VMI_LightComplexity:
				Icon = LightComplexityIcon;
				break;

			case VMI_ShaderComplexity:
				Icon = ShaderComplexityIcon;
				break;

			case VMI_QuadOverdraw:
				Icon = QuadOverdrawIcon;
				break;

			case VMI_ShaderComplexityWithQuadOverdraw:
				Icon = ShaderComplexityWithQuadOverdrawIcon;
				break;

			case VMI_PrimitiveDistanceAccuracy:
				Icon = PrimitiveDistanceAccuracyIcon;
				break;

			case VMI_MeshUVDensityAccuracy:
				Icon = MeshUVDensityAccuracyIcon;
				break;

			case VMI_MaterialTextureScaleAccuracy:
				Icon = MaterialTextureScaleAccuracyIcon;
				break;

			case VMI_RequiredTextureResolution:
				Icon = RequiredTextureResolutionIcon;
				break;

			case VMI_StationaryLightOverlap:
				Icon = LightOverlapIcon;
				break;

			case VMI_LightmapDensity:
				Icon = LightmapDensityIcon;
				break;

			case VMI_ReflectionOverride:
				Icon = ReflectionModeIcon;
				break;

			case VMI_VisualizeBuffer:
				Icon = VisualizeBufferIcon;
				break;

			case VMI_CollisionPawn:
				Icon = CollisionPawnIcon;
				break;

			case VMI_CollisionVisibility:
				Icon = CollisionVisibilityIcon;
				break;

			case VMI_LitLightmapDensity:
				break;

			case VMI_LODColoration:
				Icon = LODColorationIcon;
				break;

			case VMI_HLODColoration:
				Icon = LODColorationIcon;
				break;

			case VMI_GroupLODColoration:
				Icon = LODColorationIcon;
				break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
}

TSharedRef<SWidget> SEditorViewportViewMenu::GenerateViewMenuContent() const
{
	const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtenders);

	{
		// View modes
		{
			ViewMenuBuilder.BeginSection("ViewMode", LOCTEXT("ViewModeHeader", "View Mode") );
			{
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LitMode, NAME_None, LOCTEXT("LitViewModeDisplayName", "Lit") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.UnlitMode, NAME_None, LOCTEXT("UnlitViewModeDisplayName", "Unlit") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.WireframeMode, NAME_None, LOCTEXT("BrushWireframeViewModeDisplayName", "Wireframe") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.DetailLightingMode, NAME_None, LOCTEXT("DetailLightingViewModeDisplayName", "Detail Lighting") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LightingOnlyMode, NAME_None, LOCTEXT("LightingOnlyViewModeDisplayName", "Lighting Only") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.ReflectionOverrideMode, NAME_None, LOCTEXT("ReflectionOverrideViewModeDisplayName", "Reflections") );
				// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
				ViewMenuBuilder.AddMenuEntry(BaseViewportActions.VxgiOpacityVoxelsMode, NAME_None, LOCTEXT("VxgiOpacityVoxelsModeDisplayName", "VXGI Opacity Voxels"));
				ViewMenuBuilder.AddMenuEntry(BaseViewportActions.VxgiEmittanceVoxelsMode, NAME_None, LOCTEXT("VxgiEmittanceVoxelsModeDisplayName", "VXGI Emittance Voxels"));
				ViewMenuBuilder.AddMenuEntry(BaseViewportActions.VxgiIrradianceVoxelsMode, NAME_None, LOCTEXT("VxgiIrradianceVoxelsModeDisplayName", "VXGI Irradiance Voxels"));
#endif
				// NVCHANGE_END: Add VXGI
			}

			// Optimization
			{
				struct Local
				{
					static void BuildOptimizationMenu( FMenuBuilder& Menu, TWeakPtr< SViewportToolBar > InParentToolBar )
					{
						const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();
						Menu.AddMenuEntry( BaseViewportCommands.LightComplexityMode, NAME_None, LOCTEXT("LightComplexityViewModeDisplayName", "Light Complexity") );
						Menu.AddMenuEntry( BaseViewportCommands.LightmapDensityMode, NAME_None, LOCTEXT("LightmapDensityViewModeDisplayName", "Lightmap Density") );
						Menu.AddMenuEntry( BaseViewportCommands.StationaryLightOverlapMode, NAME_None, LOCTEXT("StationaryLightOverlapViewModeDisplayName", "Stationary Light Overlap") );
						Menu.AddMenuEntry( BaseViewportCommands.ShaderComplexityMode, NAME_None, LOCTEXT("ShaderComplexityViewModeDisplayName", "Shader Complexity") );

						if ( AllowDebugViewShaderMode(DVSM_ShaderComplexityContainedQuadOverhead) )
						{
							Menu.AddMenuEntry( BaseViewportCommands.ShaderComplexityWithQuadOverdrawMode, NAME_None, LOCTEXT("ShaderComplexityWithQuadOverdrawViewModeDisplayName", "Shader Complexity & Quads") );
						}
						if ( AllowDebugViewShaderMode(DVSM_QuadComplexity) )
						{
							Menu.AddMenuEntry( BaseViewportCommands.QuadOverdrawMode, NAME_None, LOCTEXT("QuadOverdrawViewModeDisplayName", "Quad Overdraw") );
						}

						Menu.AddMenuEntry( BaseViewportCommands.LODColorationMode, NAME_None, LOCTEXT("LODColorationViewModeDisplayName", "LOD Coloration") );

						Menu.BeginSection("TextureStreaming", LOCTEXT("TextureStreamingHeader", "Texture Streaming Accuracy") );
						if ( AllowDebugViewShaderMode(DVSM_PrimitiveDistanceAccuracy) && (!InParentToolBar.IsValid() || InParentToolBar.Pin()->IsViewModeSupported(VMI_PrimitiveDistanceAccuracy)) )
						{
							Menu.AddMenuEntry( BaseViewportCommands.TexStreamAccPrimitiveDistanceMode, NAME_None, LOCTEXT("TexStreamAccPrimitiveDistanceViewModeDisplayName", "Primitive Distance") );
						}
						if ( AllowDebugViewShaderMode(DVSM_MeshUVDensityAccuracy) && (!InParentToolBar.IsValid() || InParentToolBar.Pin()->IsViewModeSupported(VMI_MeshUVDensityAccuracy)) )
						{
							Menu.AddMenuEntry(BaseViewportCommands.TexStreamAccMeshUVDensityMode, NAME_None, LOCTEXT("TexStreamAccMeshUVDensityViewModeDisplayName", "Mesh UV Densities"));
						}
						// TexCoordScale accuracy viewmode requires shaders that are only built in the TextureStreamingBuild, which requires the new metrics to be enabled.
						if ( AllowDebugViewShaderMode(DVSM_MaterialTextureScaleAccuracy) && CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0 && (!InParentToolBar.IsValid() || InParentToolBar.Pin()->IsViewModeSupported(VMI_MaterialTextureScaleAccuracy)) )
						{
							Menu.AddMenuEntry(BaseViewportCommands.TexStreamAccMaterialTextureScaleMode, NAME_None, LOCTEXT("TexStreamAccMaterialTextureScaleViewModeDisplayName", "Material Texture Scales"));
						}
						if ( AllowDebugViewShaderMode(DVSM_RequiredTextureResolution) && (!InParentToolBar.IsValid() || InParentToolBar.Pin()->IsViewModeSupported(VMI_MaterialTextureScaleAccuracy)) )
						{
							Menu.AddMenuEntry(BaseViewportCommands.RequiredTextureResolutionMode, NAME_None, LOCTEXT("RequiredTextureResolutionModeDisplayName", "Required Texture Resolution"));
						}
						Menu.EndSection();
					}
				};

				ViewMenuBuilder.AddSubMenu( LOCTEXT("OptimizationSubMenu", "Optimization Viewmodes"), LOCTEXT("Optimization_ToolTip", "Select optimization visualizer"), FNewMenuDelegate::CreateStatic( &Local::BuildOptimizationMenu, ParentToolBar ) );
			}

			ViewMenuBuilder.EndSection();
		}

		// Auto Exposure
		{
			struct Local
			{
				static void BuildExposureMenu( FMenuBuilder& Menu )
				{
					const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

					Menu.AddMenuEntry( BaseViewportCommands.ToggleAutoExposure, NAME_None );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure4m, NAME_None, LOCTEXT("FixedExposure4m", "Fixed at Log -4 = Brightness 1/16x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure3m, NAME_None, LOCTEXT("FixedExposure3m", "Fixed at Log -3 = Brightness 1/8x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure2m, NAME_None, LOCTEXT("FixedExposure2m", "Fixed at Log -2 = Brightness 1/4x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure1m, NAME_None, LOCTEXT("FixedExposure1m", "Fixed at Log -1 = Brightness 1/2x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure0,  NAME_None, LOCTEXT("FixedExposure0",  "Fixed at Log  0") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure1p, NAME_None, LOCTEXT("FixedExposure1p", "Fixed at Log +1 = Brightness 2x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure2p, NAME_None, LOCTEXT("FixedExposure2p", "Fixed at Log +2 = Brightness 4x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure3p, NAME_None, LOCTEXT("FixedExposure3p", "Fixed at Log +3 = Brightness 8x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure4p, NAME_None, LOCTEXT("FixedExposure4p", "Fixed at Log +4 = Brightness 16x") );
				}
			};

			ViewMenuBuilder.BeginSection("Exposure");
			ViewMenuBuilder.AddSubMenu( LOCTEXT("ExposureSubMenu", "Exposure"), LOCTEXT("ExposureSubMenu_ToolTip", "Select exposure"), FNewMenuDelegate::CreateStatic( &Local::BuildExposureMenu ) );
			ViewMenuBuilder.EndSection();
		}
	}
	return ViewMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
