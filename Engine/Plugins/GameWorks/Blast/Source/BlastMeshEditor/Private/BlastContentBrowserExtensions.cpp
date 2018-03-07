#include "BlastContentBrowserExtensions.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Engine/StaticMesh.h"
#include "AssetData.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ScopedSlowTask.h"

#include "BlastMesh.h"
#include "BlastFracture.h"

#define LOCTEXT_NAMESPACE "Blast"

namespace
{
	FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
	FDelegateHandle ContentBrowserExtenderDelegateHandle;

	struct FContentBrowserSelectedAssetExtensionBase
	{
	public:
		TArray<struct FAssetData> SelectedAssets;

	public:
		virtual void Execute() {}
		virtual ~FContentBrowserSelectedAssetExtensionBase() {}
	};


	struct FCreateBlastMeshFromStaticMeshExtension : public FContentBrowserSelectedAssetExtensionBase
	{
		FCreateBlastMeshFromStaticMeshExtension()
		{
		}

		void CreateBlastMeshesFromStaticMeshes(TArray<UStaticMesh*>& Meshes)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

			FScopedSlowTask SlowTask(Meshes.Num(), LOCTEXT("CreateBlastMeshesFromStaticMeshes", "Creating Blast meshes"));
			TArray<UObject*> ObjectsToSync;
			for (UStaticMesh* StaticMesh : Meshes)
			{
				SlowTask.EnterProgressFrame();
				FString Name;
				FString PackageName;

				const FString DefaultSuffix = TEXT("_Blast");
				AssetToolsModule.Get().CreateUniqueAssetName(StaticMesh->GetOutermost()->GetName(), DefaultSuffix, /*out*/ PackageName, /*out*/ Name);
				const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

				if (UBlastMesh* NewAsset = Cast<UBlastMesh>(AssetToolsModule.Get().CreateAsset(Name, PackagePath, UBlastMesh::StaticClass(), nullptr)))
				{
					auto BlastFracture = FBlastFracture::GetInstance();
					BlastFracture->FinishFractureSession(BlastFracture->StartFractureSession(NewAsset, StaticMesh));
					ObjectsToSync.Add(NewAsset);
				}
			}

			if (ObjectsToSync.Num() > 0)
			{
				ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
			}
		}

		virtual void Execute() override
		{
			TArray<UStaticMesh*> Meshes;
			for (const FAssetData& AssetData : SelectedAssets)
			{
				if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
				{
					Meshes.Add(Mesh);
				}
			}

			CreateBlastMeshesFromStaticMeshes(Meshes);
		}
	};


	class FBlastContentBrowserExtensions_Impl
	{
	public:
		static void ExecuteSelectedContentFunctor(TSharedPtr<FContentBrowserSelectedAssetExtensionBase> SelectedAssetFunctor)
		{
			SelectedAssetFunctor->Execute();
		}

		static void CreateStaticMeshActions(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
		{
			TSharedPtr<FCreateBlastMeshFromStaticMeshExtension> MeshCreatorFunctor = MakeShareable(new FCreateBlastMeshFromStaticMeshExtension());
			MeshCreatorFunctor->SelectedAssets = SelectedAssets;

			FUIAction Action_CreateBlastMeshFromStaticMesh(
				FExecuteAction::CreateStatic(&FBlastContentBrowserExtensions_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(MeshCreatorFunctor)));

			//TODO add Blast styleset
			const FName StyleSetName = FEditorStyle::GetStyleSetName();

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CB_Extension_StaticMesh_CreateBlastMesh", "Create Blast Mesh"),
				LOCTEXT("CB_Extension_StaticMesh_CreateBlastMesh_Tooltip", "Create Blast meshes from selected static meshes"),
				FSlateIcon(StyleSetName, "ClassIcon.DestructibleComponent"),
				Action_CreateBlastMeshFromStaticMesh,
				NAME_None,
				EUserInterfaceActionType::Button);
		}


		static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
		{
			TSharedRef<FExtender> Extender(new FExtender());

			// Run thru the assets to determine if any meet our criteria
			bool bAnyStaticMeshes = false;
			for (const FAssetData& Asset : SelectedAssets)
			{
				bAnyStaticMeshes = bAnyStaticMeshes || (Asset.AssetClass == UStaticMesh::StaticClass()->GetFName());
			}

			if (bAnyStaticMeshes)
			{
				Extender->AddMenuExtension(
					"GetAssetActions",
					EExtensionHook::After,
					nullptr,
					FMenuExtensionDelegate::CreateStatic(&FBlastContentBrowserExtensions_Impl::CreateStaticMeshActions, SelectedAssets));
			}

			return Extender;
		}

		static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
			return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		}
	};
}

void FBlastContentBrowserExtensions::InstallHooks()
{
	ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBlastContentBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FBlastContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FBlastContentBrowserExtensions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FBlastContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate){ return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

#undef LOCTEXT_NAMESPACE
