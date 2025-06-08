// Fill out your copyright notice in the Description page of Project Settings.

#include "EffectDisplayActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstanceController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

AEffectDisplayActor::AEffectDisplayActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RotationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RotationRoot"));
	RotationRoot->SetupAttachment(RootComponent);
	Playlist.Reserve(PlaylistReserveCapacity);
	PrimaryActorTick.bCanEverTick = true;
}

#if EFFECT_DISPLAY_ENABLED

void AEffectDisplayActor::LoadAdditionalNiagaraSystems()
{
	if(AdditionalNiagaraFolderPath.IsEmpty())
	{
		return;
	}
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(*AdditionalNiagaraFolderPath);
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = false;

	// アセットの検索
	TArray<FAssetData> NiagaraAssetArray{};
	NiagaraAssetArray.Reserve(PlaylistReserveCapacity);
	AssetRegistry.GetAssets(Filter, NiagaraAssetArray);

	if(NiagaraAssetArray.IsEmpty())
	{
		UE_LOG(LogTemp, Log,TEXT("not found niagara system in %s "), *AdditionalNiagaraFolderPath);
	}
	for(const auto& Asset : NiagaraAssetArray)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset.GetAsset());
		if(System)
		{
			Playlist.Add(System);	
		}
	}
}


void AEffectDisplayActor::BeginPlay()
{
	Super::BeginPlay();
	const FVector Offset = GetActorForwardVector() * EffectPlaceOffset.X
						+ GetActorRightVector() * EffectPlaceOffset.Y
						+ GetActorUpVector() * EffectPlaceOffset.Z;
	const FVector Location = GetActorLocation() + Offset;
	RotationRoot->SetWorldLocation(GetActorLocation() + Offset);

	//初期値が無効になっているものがあれば取り除く
	Playlist.RemoveAll([](const TObjectPtr<UNiagaraSystem>& System)
	{
		return !IsValid(System);
	});
	LoadAdditionalNiagaraSystems();
	if(IsAutoPlay && !Playlist.IsEmpty())
	{
		PlayEffect();
	}
}

void AEffectDisplayActor::PlayEffect()
{
	if(IsPlaying())
	{
		StopCurrentPlayEffect();
	}
	CurrentPlayIndex = InvalidPlayIndex;
	PlayNext();
}

void AEffectDisplayActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!NiagaraComponent)
	{
		return;
	}
	
	if(!NiagaraComponent->IsActive())
	{
		StopCurrentPlayEffect();
		PlayNext();
		return;
	}
	RotationNiagaraSystem(DeltaTime);

	const auto NiagaraSystem = NiagaraComponent->GetAsset();
	if(!NiagaraSystem)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("AEffectDisplayActor::Tick NiagaraSystem is nullptr"));
		return;
	}
	const auto SystemInstanceController = NiagaraComponent->GetSystemInstanceController();
	if(!SystemInstanceController.IsValid())
	{
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor::Tick SystemInstanceController Invalid"));
		return;
	}
	const auto CurrentAge =SystemInstanceController->GetAge();
	if(CurrentAge >= PlayInterval)
	{
		StopCurrentPlayEffect();
		PlayNext();
	}
}

void AEffectDisplayActor::StopCurrentPlayEffect()
{
	if (NiagaraComponent && NiagaraComponent->IsActive())
	{
		NiagaraComponent->DeactivateImmediate();
	}
	if (NiagaraComponent)
	{
		NiagaraComponent->DestroyComponent(); 
		NiagaraComponent = nullptr;
	}
}

bool AEffectDisplayActor::PlayNext()
{
	CurrentPlayIndex++;
	if(CurrentPlayIndex >= Playlist.Num())
	{
		if(IsLoop)
		{
			CurrentPlayIndex = 0;
		}
		else
		{
			CurrentPlayIndex = InvalidPlayIndex;
			UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Finish Play Effect"));
			return false;
		}
	}
	UNiagaraSystem* PlaySystem = Playlist[CurrentPlayIndex];
	NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		PlaySystem,
		RotationRoot,
		NAME_None,
		FVector::Zero(),
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		true);
	if(!NiagaraComponent)
	{
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Finish Play Effect"));
		return false;
	}
	NiagaraComponent->SetAutoDestroy(true);
	UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Begin Play %s"),*PlaySystem->GetName());
	return true;
}

bool AEffectDisplayActor::IsPlaying() const
{
	if(!NiagaraComponent)
	{
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor::IsPlaying NiagaraComponent is nullptr"));
		return false;
	}
		
	return NiagaraComponent->IsActive() || CurrentPlayIndex > 0;
}

void AEffectDisplayActor::RotationNiagaraSystem(float DeltaTime)const
{
	FRotator CurrentRotation = RotationRoot->GetRelativeRotation();
	CurrentRotation.Yaw += RotateSpeed * DeltaTime;
	RotationRoot->SetRelativeRotation(CurrentRotation);
}

#endif // UE_BUILD_DEVELOPMENT
