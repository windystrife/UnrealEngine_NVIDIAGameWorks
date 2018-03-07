//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IDetailCustomization.h"
#include "Input/Reply.h"

class UPhononSourceComponent;
class IDetailLayoutBuilder;

namespace SteamAudio
{
	class FPhononSourceComponentDetails : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance();

	private:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

		FReply OnBakePropagation();
		bool IsBakeEnabled() const;

		TWeakObjectPtr<UPhononSourceComponent> PhononSourceComponent;
	};
}
