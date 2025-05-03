// Fill out your copyright notice in the Description page of Project Settings.


#include "EffectDisplayActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"

AEffectDisplayActor::AEffectDisplayActor()
{
	PlaylistArray.Reserve(QueueCapacity);
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

// Called when the game starts or when spawned
void AEffectDisplayActor::BeginPlay()
{
	Super::BeginPlay();
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



