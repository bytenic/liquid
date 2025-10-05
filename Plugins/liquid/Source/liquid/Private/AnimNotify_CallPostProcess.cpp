// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_CallPostProcess.h"

#include "PostProcessCallSubsystem.h"

UAnimNotify_CallPostProcess::UAnimNotify_CallPostProcess()
{
}

void UAnimNotify_CallPostProcess::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                         const FAnimNotifyEventReference& EventReference)
{
	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}
	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}
	auto CallSystem = World->GetSubsystem<UPostProcessCallSubsystem>();
	if (!CallSystem)
	{
		return;
	}
	CallSystem->PlayTransientPostProcess(PostProcessRowName,[this, MeshComp, Animation](UMaterialInstanceDynamic* DynamicInstance)
	{
		InitializeMaterialParameters(MeshComp, DynamicInstance);
	});
	
}

void UAnimNotify_CallPostProcess::InitializeMaterialParameters(USkeletalMeshComponent* MeshComp,
	UMaterialInstanceDynamic* MID)
{
	if (!MID || !MeshComp)
	{
		return;
	}
	for (const auto& Setting : ParamSettings)
	{
		switch (Setting.ParamType)
		{
		case EPostProcessCallParamType::ActorLocation:
			if (AActor* Owner = MeshComp->GetOwner())
			{
				FVector4 WorldLocation = FVector4(Owner->GetActorLocation(), 1.0f);
				MID->SetVectorParameterValue(Setting.ParamName, WorldLocation);
			}
			break;

			case EPostProcessCallParamType::SocketLocation:
			if (!Setting.SocketName.IsNone())
			{
				if (!MeshComp->DoesSocketExist(Setting.SocketName))
				{
					UE_LOG(LogTemp, Warning, TEXT("[UAnimNotify_CallPostProcess] Socket '%s' not found"), *Setting.SocketName.ToString());
					continue;
				}
				FVector4 SocketLocation = FVector4(MeshComp->GetSocketLocation(Setting.SocketName), 1.0f);
				MID->SetVectorParameterValue(Setting.ParamName, SocketLocation);
			}
			break;
		}
	}
}
