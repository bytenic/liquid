// Fill out your copyright notice in the Description page of Project Settings.

#include "EffectDisplayActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstanceController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/KismetSystemLibrary.h"

#if WITH_EDITOR
#include "Kismet/KismetArrayLibrary.h"
#endif
AEffectDisplayActor::AEffectDisplayActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RotationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RotationRoot"));
	RotationRoot->SetupAttachment(RootComponent);
	PlaceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlaceRoot"));
	PlaceRoot->SetupAttachment(RotationRoot);
	Playlist.Reserve(PlaylistReserveCapacity);

#if EFFECT_DISPLAY_ENABLED
	PrimaryActorTick.bCanEverTick = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif
}

#if EFFECT_DISPLAY_ENABLED

void AEffectDisplayActor::BeginLoadAsync()
{
	if (!AdditionalNiagaraFolderPath.IsEmpty())
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FARFilter Filter;
		Filter.PackagePaths.Add(*AdditionalNiagaraFolderPath);
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = false;
		Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());

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
			TSoftObjectPtr<UNiagaraSystem> SoftPtr(Asset.ToSoftObjectPath());
			Playlist.Add(SoftPtr);	
		}
	}
	LoadNiagaraSystemAsync();
}

void AEffectDisplayActor::LoadNiagaraSystemAsync()
{
	if (!Playlist.IsValidIndex(CurrentLoadingIndex))
	{
		return;
	}
	FStreamableManager& Manager = UAssetManager::GetStreamableManager();
	auto& SoftPtr = Playlist[CurrentLoadingIndex];
	CurrentLoadingHandle = Manager.RequestAsyncLoad(
	SoftPtr.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([this]()
		{
			auto& SoftPtr = Playlist[CurrentLoadingIndex];
			UNiagaraSystem* LoadedNiagara =  Playlist[CurrentLoadingIndex].Get();
			const bool IsSuccess = Playlist[CurrentLoadingIndex].IsValid() && LoadedNiagara != nullptr;
			if (!IsSuccess)
			{
				UE_LOG(LogTemp, Warning,
				TEXT("[AEffectDisplayActor] Retry Load : %s"), *SoftPtr.ToSoftObjectPath().ToString());
				LoadNiagaraSystemAsync();
				return;
			}
			
			if (LoadedNiagara)
			{
				LoadedPlayList.Add(LoadedNiagara);
				UE_LOG(LogTemp, Log,
				   TEXT("[AEffectDisplayActor::LoadNiagaraSystemAsync] Loaded Niagara %s"), *SoftPtr.ToSoftObjectPath().ToString());
			}
			else
			{
				UE_LOG(LogTemp, Error,
				   TEXT("[AEffectDisplayActor::LoadNiagaraSystemAsync] Failed to load Niagara %s"),
				   *SoftPtr.ToSoftObjectPath().ToString());
			}
			CurrentLoadingIndex++;
			if (CurrentLoadingIndex < Playlist.Num())
			{
				LoadNiagaraSystemAsync();
			}
		}));
}

void AEffectDisplayActor::BeginPlay()
{
	Super::BeginPlay();
	const FVector Offset = GetActorForwardVector() * PlaceOffset.X
						+ GetActorRightVector() * PlaceOffset.Y
						+ GetActorUpVector() * PlaceOffset.Z;
	const FVector RotateLocation = GetActorLocation() + Offset;
	RotationRoot->SetWorldLocation(RotateLocation);

	const FVector Radius(RotationRadius,.0,.0);
	PlaceRoot->SetWorldLocation(RotateLocation + Radius);
	
	//初期値が無効になっているものがあれば取り除く
	Playlist.RemoveAll([](const TSoftObjectPtr<UNiagaraSystem>& System)
	{
		return System.IsNull();
	});
	BeginLoadAsync();
}

void AEffectDisplayActor::Destroyed()
{
	Super::Destroyed();
	if (CurrentLoadingHandle.IsValid())
	{
		CurrentLoadingHandle->CancelHandle();
	}
}

bool AEffectDisplayActor::ShouldStartNextEffect() 
{
	if (NiagaraComponent)
		return false;

	if (CurrentPlayIndex >= CurrentLoadingIndex)
	{
		if (CurrentPlayIndex  == Playlist.Num() - 1 && IsLoop)
		{
			CurrentPlayIndex = InvalidPlayIndex;
			return true;
		}
		return false; 
	}

	return true;
}

void AEffectDisplayActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (NiagaraComponent)
	{
		if (!NiagaraComponent->IsActive())
		{
			//一旦次フレームで再生判定を行う
			StopCurrentPlayEffect();
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
		}
	}
	if (ShouldStartNextEffect())
	{
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
	if (LoadedPlayList.IsEmpty())
	{
		return false;
	}
	CurrentPlayIndex++;
	if(CurrentPlayIndex >= LoadedPlayList.Num())
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
	UNiagaraSystem* PlaySystem = LoadedPlayList[CurrentPlayIndex];
	NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		PlaySystem,
		PlaceRoot,
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

#if WITH_EDITOR
	UKismetSystemLibrary::PrintString(
		this,
		FString::Printf(TEXT("%s: %s"), *GetActorLabel(), *PlaySystem->GetName()),
		true,
		true,
		FLinearColor::Yellow,
		2.0f);
	
#else
	UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor Begin Play %s"),*PlaySystem->GetName());
#endif
	return true;
}

bool AEffectDisplayActor::IsPlaying() const
{
	if(!NiagaraComponent)
	{
		UE_LOG(LogTemp, Log,TEXT("AEffectDisplayActor::IsPlaying NiagaraComponent is nullptr"));
		return false;
	}
		
	return NiagaraComponent->IsActive() || CurrentPlayIndex != InvalidPlayIndex;
}

void AEffectDisplayActor::RotationNiagaraSystem(float DeltaTime)const
{
	FRotator CurrentRotation = RotationRoot->GetRelativeRotation();
	CurrentRotation.Yaw += RotateSpeed * DeltaTime;
	RotationRoot->SetRelativeRotation(CurrentRotation);
}

#endif // UE_BUILD_DEVELOPMENT
