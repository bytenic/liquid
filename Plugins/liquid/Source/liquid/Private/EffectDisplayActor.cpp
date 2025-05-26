// Fill out your copyright notice in the Description page of Project Settings.


#include "EffectDisplayActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

AEffectDisplayActor::AEffectDisplayActor()
{
	PlaylistArray.Reserve(QueueCapacity);
	PrimaryActorTick.bCanEverTick = true;
}

void AEffectDisplayActor::PlayEffect(TArray<UNiagaraSystem*> PlayList)
{
	if(IsPlaying())
	{
		StopCurrentPlayEffect();
		ClearPlaylistQueue();
	}
	PlaylistArray = MoveTemp(PlayList);
	CurrentPlayIndex = InvalidPlayIndex;
	
	PlayNext();
}

void AEffectDisplayActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!NiagaraComponent)
		return;

	RotationNiagaraSystem(DeltaTime);

	if(!NiagaraComponent->IsActive())
		return;

	const auto NiagaraSystem = NiagaraComponent->GetAsset();
	if(!NiagaraSystem)
	{
		return;
	}
	
	const auto SystemInstanceController = NiagaraComponent->GetSystemInstanceController();
	if(!SystemInstanceController.IsValid())
	{
		return;
	}
	const auto CurrentAge =SystemInstanceController->GetAge();
	if(CurrentAge >= PlayInterval)
	{
		NiagaraComponent->DestroyComponent();
		PlayNext();
	}
}

// Called when the game starts or when spawned
void AEffectDisplayActor::BeginPlay()
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
	if(IsAutoPlay)
	{
		ExecuteAllEffectSequential();
	}
}

void AEffectDisplayActor::OnNiagaraSystemFinished(UNiagaraComponent* InComp)
{
	UE_LOG(LogTemp, Verbose, TEXT("OnNiagaraSystemFinished Called"));
	if(InComp)
	{
		InComp->DestroyComponent();		
	}
	NiagaraComponent = nullptr;
	PlayNext();
}

void AEffectDisplayActor::StopCurrentPlayEffect()
{
	if(NiagaraComponent || !NiagaraComponent->IsActive())
	{
		NiagaraComponent->DeactivateImmediate();
	}
	NiagaraComponent = nullptr;
}

void AEffectDisplayActor::ClearPlaylistQueue()
{
	PlaylistArray.Empty();
}

bool AEffectDisplayActor::PlayNext()
{
	CurrentPlayIndex++;
	if(CurrentPlayIndex >= PlaylistArray.Num())
	{
		CurrentPlayIndex = InvalidPlayIndex;
		return false;
	}
	UNiagaraSystem* PlaySystem = PlaylistArray[CurrentPlayIndex];
	const FVector Offset = GetActorForwardVector() * EffectPlaceOffset.X
							+ GetActorRightVector() * EffectPlaceOffset.Y
							+ GetActorUpVector() * EffectPlaceOffset.Z;
	const FVector Location = GetActorLocation() + Offset;
	NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		PlaySystem,Location,
		FRotator::ZeroRotator,
		FVector::One(),
		true,
		true);
	if(!NiagaraComponent)
		return false;
	NiagaraComponent->OnSystemFinished.AddDynamic(this,
		&AEffectDisplayActor::OnNiagaraSystemFinished);
	return true;
}

bool AEffectDisplayActor::IsPlaying() const
{
	if(!NiagaraComponent)
		return false;
	return !NiagaraComponent->IsActive() || CurrentPlayIndex > 0;
}

void AEffectDisplayActor::RotationNiagaraSystem(float DeltaTime)
{
	FRotator CurrentRotation = NiagaraComponent->GetRelativeRotation();
	CurrentRotation.Yaw += RotateSpeed * DeltaTime;
	NiagaraComponent->SetRelativeRotation(CurrentRotation);
}
