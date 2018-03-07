//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;
class IDetailChildrenBuilder;
class IPropertyHandle;
class APhononScene;

namespace SteamAudio
{
	class FPhononSceneDetails : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance();

	private:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

		FText GetSceneDataSizeText() const;

		FReply OnExportScene();
		TWeakObjectPtr<APhononScene> PhononSceneActor;
	};
}