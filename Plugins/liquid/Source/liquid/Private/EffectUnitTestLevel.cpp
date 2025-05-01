// Fill out your copyright notice in the Description page of Project Settings.


#include "EffectUnitTestLevel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

void AEffectUnitTestLevel::BeginPlay()
{
	Super::BeginPlay();

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(*NiagaraRootPath);
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = false;

	// アセットの検索
	AssetRegistry.GetAssets(Filter, NiagaraAssetArray);
	if(NiagaraAssetArray.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,TEXT("not found niagara system in %s "), *NiagaraRootPath);
	}
	else
	{
		for(const auto& it : NiagaraAssetArray)
		{
			UE_LOG(LogTemp, Verbose,TEXT("%s"), *it.AssetName.ToString());
		}
	}
	ExecuteTestSequential();
}
