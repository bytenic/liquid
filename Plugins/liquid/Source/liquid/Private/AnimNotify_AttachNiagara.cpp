// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_AttachNiagara.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UAnimNotify_AttachNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
}

UNiagaraComponent* UAnimNotify_AttachNiagara::SpawnNiagara(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation)
{
	if (!IsValid(MeshComp) || !!IsValid(Animation) || !IsValid(NiagaraSystem))
	{
		return nullptr;
	}
	//note: 元のAPIのコメントがちょっと怪しい
	if (NiagaraSystem->IsLooping())
	{
		return NiagaraComponent.Get();
	}

	UNiagaraComponent* ReturnComp = nullptr;
	if (AttachType == EAttachNiagaraLocationType::SocketLocation)
	{
		ReturnComp = UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, MeshComp, AttachSocketName, LocationOffset, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		const FTransform MeshTransform = MeshComp->GetRelativeTransform();
		ReturnComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp->GetWorld(), NiagaraSystem, MeshTransform.TransformPosition(LocationOffset), FRotator::ZeroRotator, FVector(1.0f),true);
	}

	if (ReturnComp != nullptr)
	{
		//ReturnComp->SetUsingAbsoluteScale(bAhbsoluteScale);
		//ReturnComp->SetRelativeScale3D_Direct(Scale);
	}
	//ReturnComp->AttachToComponent()
	return ReturnComp;
}
