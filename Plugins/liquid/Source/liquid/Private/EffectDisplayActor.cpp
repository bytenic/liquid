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
	RotationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RotationRoot"));
	//RotationRoot->SetupAttachment(RootComponent);
	PlaylistArray.Reserve(QueueCapacity);
	PrimaryActorTick.bCanEverTick = true;
	
}

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
		UE_LOG(LogTemp, Log,TEXT("not found niagara system in %s "), *NiagaraRootPath);
	}
	else
	{
		for(const auto& it : NiagaraAssetArray)
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *it.AssetName.ToString());
		}
	}
	if(IsAutoPlay)
	{
		ExecuteAllEffectSequential();
	}
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
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor::Tick NiagaraSystem is nullptr"));
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
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Finish Play Effect"));
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
	{
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Finish Play Effect"));
		return false;
	}
	NiagaraComponent->SetAutoDestroy(true);
	NiagaraComponent->AttachToComponent(RotationRoot, FAttachmentTransformRules::KeepWorldTransform);
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

void AEffectDisplayActor::RotationNiagaraSystem(float DeltaTime)
{
	FRotator CurrentRotation = RotationRoot->GetRelativeRotation();
	CurrentRotation.Yaw += RotateSpeed * DeltaTime;
	RotationRoot->SetRelativeRotation(CurrentRotation);
}
