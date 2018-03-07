#include "AssetTypeActions_BlastMesh.h"
#include "SlateIcon.h"
#include "Styling/SlateIconFinder.h"
#include "BlastMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "IBlastMeshEditorModule.h"

#define LOCTEXT_NAMESPACE "Blast"

UClass* FAssetTypeActions_BlastMesh::GetSupportedClass() const
{
	return UBlastMesh::StaticClass();
}

void FAssetTypeActions_BlastMesh::GetActions(const TArray<UObject *>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Meshes = GetTypedWeakObjectPtrs<UBlastMesh>(InObjects);

	MenuBuilder.AddMenuEntry(LOCTEXT("BlastMesh_ViewSkeletalMesh", "Open Skeletal Mesh"),
							LOCTEXT("BlastMesh_ViewSkeletalMesh_Tooltop", "View the skeletal mesh part of this asset"),
							FSlateIconFinder::FindIconForClass(USkeletalMesh::StaticClass()),
							FUIAction(FExecuteAction::CreateLambda([Meshes]()
							{
								TArray<UObject*> Assets;
								for (TWeakObjectPtr<UBlastMesh> BlastMesh : Meshes)
								{
									if (BlastMesh.IsValid() && BlastMesh->Mesh)
									{
										Assets.Add(BlastMesh->Mesh);
									}
								}
								FAssetEditorManager::Get().OpenEditorForAssets(Assets);
							})));

	MenuBuilder.AddMenuEntry(LOCTEXT("BlastMesh_ViewSkeleton", "Open Skeleton"),
							LOCTEXT("BlastMesh_ViewSkeleton_Tooltop", "View the skeleton part of this asset"),
							FSlateIconFinder::FindIconForClass(USkeleton::StaticClass()),
							FUIAction(FExecuteAction::CreateLambda([Meshes]()
							{
								TArray<UObject*> Assets;
								for (TWeakObjectPtr<UBlastMesh> BlastMesh : Meshes)
								{
									if (BlastMesh.IsValid() && BlastMesh->Mesh)
									{
										Assets.Add(BlastMesh->Skeleton);
									}
								}
								FAssetEditorManager::Get().OpenEditorForAssets(Assets);
							})));

	MenuBuilder.AddMenuEntry(LOCTEXT("BlastMesh_ViewPhysicsAsset", "Open Physics Asset"),
							LOCTEXT("BlastMesh_ViewPhysicsAsset_Tooltop", "View the physics asset part of this asset"),
							FSlateIconFinder::FindIconForClass(UPhysicsAsset::StaticClass()),
							FUIAction(FExecuteAction::CreateLambda([Meshes]()
							{
								TArray<UObject*> Assets;
								for (TWeakObjectPtr<UBlastMesh> BlastMesh : Meshes)
								{
									if (BlastMesh.IsValid() && BlastMesh->Mesh)
									{
										Assets.Add(BlastMesh->PhysicsAsset);
									}
								}
								FAssetEditorManager::Get().OpenEditorForAssets(Assets);
							})));
}

void FAssetTypeActions_BlastMesh::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<UBlastMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			IBlastMeshEditorModule& BlastMeshEditorModule = FModuleManager::LoadModuleChecked<IBlastMeshEditorModule>("BlastMeshEditor");
			TSharedRef< IBlastMeshEditor > NewBlastMeshEditor = BlastMeshEditorModule.CreateBlastMeshEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Mesh);
		}
	}
}

#undef LOCTEXT_NAMESPACE