//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "ISteamAudioEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSteamAudioEditor, Log, All);

class FComponentVisualizer;
class FSlateStyleSet;

namespace SteamAudio
{
	class FBakeIndirectWindow;

	class FSteamAudioEditorModule : public ISteamAudioEditorModule
	{
	public:
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		TSharedRef<FExtender> OnExtendLevelEditorBuildMenu(const TSharedRef<FUICommandList> CommandList);
		void CreateBuildMenu(FMenuBuilder& Builder);

		void BakeIndirect();
		bool IsReadyToBakeIndirect() const;

		void RegisterComponentVisualizer(const FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);

	private:
		TSharedPtr<FSlateStyleSet> SteamAudioStyleSet;
		TSharedPtr<FBakeIndirectWindow> BakeIndirectWindow;
		TArray<FName> RegisteredComponentClassNames;
	};
}
