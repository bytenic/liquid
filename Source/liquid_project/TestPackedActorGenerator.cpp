// Fill out your copyright notice in the Description page of Project Settings.


#include "TestPackedActorGenerator.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "EditorDirectories.h"
#include "IContentBrowserSingleton.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PackedLevelActor/PackedLevelActor.h"

#define LOCTEXT_NAMESPACE "FTestPackedActorGenerator"

class UBlueprintFactory;
class FContentBrowserModule;

UBlueprint* CreatePackedLevelActorBlueprint(TSoftObjectPtr<UBlueprint> InBlueprintAsset, TSoftObjectPtr<UWorld> InWorldAsset, bool bInCompile)
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = APackedLevelActor::StaticClass();
	BlueprintFactory->bSkipClassPicker = true;

	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(BlueprintFactory);
	if (BlueprintFactory->ConfigureProperties())
	{
		FString PackageDir = FPaths::GetPath(InBlueprintAsset.GetLongPackageName());
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, PackageDir);

		if (UBlueprint* NewBP = Cast<UBlueprint>(AssetTools.CreateAsset(InBlueprintAsset.GetAssetName(), PackageDir, UBlueprint::StaticClass(), BlueprintFactory, FName("Create LevelInstance Blueprint"))))
		{
			APackedLevelActor* CDO = CastChecked<APackedLevelActor>(NewBP->GeneratedClass->GetDefaultObject());
			CDO->SetWorldAsset(InWorldAsset);

			if (bInCompile)
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP, EBlueprintCompileOptions::None);
			}

			AssetTools.SyncBrowserToAssets(TArray<UObject*>{ NewBP });

			return NewBP;
		}
	}

	return nullptr;
}

bool UTestPackedActorGenerator::GenerateActor()
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = FText::Format(LOCTEXT("Save as", "Save As"));
	SaveAssetDialogConfig.DefaultPath = FPaths::GetPath(InBlueprintAsset.GetLongPackageName());
	SaveAssetDialogConfig.DefaultAssetName = InBlueprintAsset.GetAssetName();
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		TSoftObjectPtr<UBlueprint> ExistingBPAsset = TSoftObjectPtr<UBlueprint>(FSoftObjectPath(SaveObjectPath));

		if (UBlueprint* BP = ExistingBPAsset.LoadSynchronous())
		{
			return BP;
		}
				
		return CreatePackedLevelActorBlueprint(ExistingBPAsset, InWorldAsset, true);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
