// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_AttachNiagara.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Misc/UObjectToken.h"

void UAnimNotify_AttachNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	InitializeNiagara(MeshComp, Animation);
	SetUpDeactivate(MeshComp, Animation);
	SetUpDestroy(MeshComp, Animation);
}

void UAnimNotify_AttachNiagara::BeginDestroy()
{
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UAnimNotify_AttachNiagara::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimNotify_AttachNiagara::ValidateAssociatedAssets()
{
	//note: UAnimNotify_PlayNiagaraEffectから引用
	static const FName NAME_AssetCheck("AssetCheck");

	if ((NiagaraSystem != nullptr) && (NiagaraSystem->IsLooping()))
	{
		UObject* ContainingAsset = GetContainingAsset();

		FMessageLog AssetCheckLog(NAME_AssetCheck);

		const FText MessageLooping = FText::Format(
			NSLOCTEXT("AnimNotify", "NiagaraSystem_ShouldNotLoop",
			          "Niagara system {0} used in anim notify for asset {1} is set to looping, but the slot is a one-shot (it won't be played to avoid leaking a component per notify)."),
			FText::AsCultureInvariant(NiagaraSystem->GetPathName()),
			FText::AsCultureInvariant(ContainingAsset->GetPathName()));
		AssetCheckLog.Error()
		             ->AddToken(FUObjectToken::Create(ContainingAsset))
		             ->AddToken(FTextToken::Create(MessageLooping));

		if (GIsEditor)
		{
			AssetCheckLog.Notify(MessageLooping, EMessageSeverity::Warning, /*bForce=*/ true);
		}
	}
}
#endif

UNiagaraComponent* UAnimNotify_AttachNiagara::SpawnNiagara(USkeletalMeshComponent* MeshComp,
                                                           const UAnimSequenceBase* Animation) const
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	if (!IsValid(Animation))
	{
		return nullptr;
	}
	if (!NiagaraSystem)
	{
		return nullptr;
	}
	if (NiagaraSystem->IsLooping())
	{
		if (IsValid(NiagaraComponent))
			return NiagaraComponent.Get();
	}

	const FTransform MeshTransform = IsAttach
		                                 ? MeshComp->GetSocketTransform(AttachSocketName, RTS_World)
		                                 : MeshComp->GetRelativeTransform();
	UNiagaraComponent* ReturnComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		MeshComp->GetWorld(), NiagaraSystem, MeshTransform.TransformPosition(LocationOffset), FRotator::ZeroRotator,
		FVector(1.0f), true);

	if (IsAttach)
	{
		constexpr EAttachmentRule FixScalingRule = EAttachmentRule::KeepRelative;
		FAttachmentTransformRules AttachRule{LocationRule, RotationRule, FixScalingRule, InWeldSimulatedBodies};
		//ParentSocketNameがNoneまたは存在しないソケットの場合は親のTransformに直接アタッチされる
		ReturnComp->AttachToComponent(MeshComp, AttachRule, AttachSocketName);
	}
	return ReturnComp;
}

void UAnimNotify_AttachNiagara::SetFloatParametersToNiagara(UNiagaraComponent* InNiagaraComponent)
{
	if (!IsValid(InNiagaraComponent))
	{
		return;
	}

	for (const auto& Param : InitialFloatParameters)
	{
		InNiagaraComponent->SetVariableFloat(Param.Key, Param.Value);
	}
}

void UAnimNotify_AttachNiagara::SetSocketLocationToNiagara(UNiagaraComponent* InNiagaraComponent,
                                                           USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(InNiagaraComponent) || !IsValid(MeshComp))
	{
		return;
	}
	for (const auto& Param : InitialSocketLocationParameters)
	{
		const FTransform SocketTransform = MeshComp->GetSocketTransform(Param.Key);
		InNiagaraComponent->SetVariablePosition(Param.Value, SocketTransform.GetLocation());
	}
}

void UAnimNotify_AttachNiagara::InitializeNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (DelayActivateTime > .0f)
	{
		FWeakObjectPtr WeakMeshComp = MeshComp;
		FWeakObjectPtr WeakAnimation = Animation;
		TWeakObjectPtr<UAnimNotify_AttachNiagara> WeakThis = this;
		if (auto World = MeshComp->GetWorld())
		{
			FTimerHandle DelayActivateHandle;
			World->GetTimerManager().SetTimer(DelayActivateHandle, [WeakMeshComp, WeakAnimation, WeakThis]()
			{
				if (auto* ThisPtr = WeakThis.Get())
				{
					if (auto* MeshCompPtr = Cast<USkeletalMeshComponent>(WeakMeshComp.Get()))
					{
						if (auto* AnimationPtr = Cast<UAnimSequenceBase>(WeakAnimation.Get()))
						{
							ThisPtr->ActivateNiagara(MeshCompPtr, AnimationPtr);
						}
					}
				}
			}, DelayActivateTime, false);
		}
	}
	else
	{
		ActivateNiagara(MeshComp, Animation);
	}
}

void UAnimNotify_AttachNiagara::ActivateNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	NiagaraComponent = SpawnNiagara(MeshComp, Animation);
	SetFloatParametersToNiagara(NiagaraComponent);
	SetSocketLocationToNiagara(NiagaraComponent, MeshComp);
}

void UAnimNotify_AttachNiagara::SetUpDeactivate(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (!IsValid(MeshComp) || !IsValid(NiagaraComponent))
	{
		return;
	}

	if (DeactiveTime > .0f)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComponent = NiagaraComponent;
		if (auto World = MeshComp->GetWorld())
		{
			FTimerHandle DeactivateHandle;
			World->GetTimerManager().SetTimer(DeactivateHandle, [WeakNiagaraComponent]()
			{
				if (auto* NiagaraComponentPtr = WeakNiagaraComponent.Get())
				{
					NiagaraComponentPtr->Deactivate();
				}
			}, DeactiveTime, false);
		}
	}
}

void UAnimNotify_AttachNiagara::SetUpDestroy(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (!IsValid(MeshComp) || !IsValid(NiagaraComponent))
	{
		return;
	}

	if (DestroyTime > .0f)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComponent = NiagaraComponent;
		if (auto World = MeshComp->GetWorld())
		{
			FTimerHandle DestroyHandle;
			World->GetTimerManager().SetTimer(DestroyHandle, [WeakNiagaraComponent]()
			{
				if (auto* NiagaraComponentPtr = WeakNiagaraComponent.Get())
				{
					NiagaraComponentPtr->Deactivate();
					NiagaraComponentPtr->DestroyComponent();
				}
			}, DestroyTime, false);
		}
	}
}
