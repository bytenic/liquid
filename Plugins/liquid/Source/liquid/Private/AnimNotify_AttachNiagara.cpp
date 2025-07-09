// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_AttachNiagara.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Misc/UObjectToken.h"

/**
* @brief アニメーション通知イベントが発生した時に呼び出される
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
* @param EventReference アニメーション通知イベントの参照
*/
void UAnimNotify_AttachNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
									   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	InitializeNiagara(MeshComp, Animation);
}

/**
* @brief オブジェクトが破棄される時に呼び出される
*/
void UAnimNotify_AttachNiagara::BeginDestroy()
{
	Super::BeginDestroy();
}

#if WITH_EDITOR
/**
* @brief エディタでプロパティが変更された時に呼び出される
* @param PropertyChangedEvent プロパティ変更イベント
*/
void UAnimNotify_AttachNiagara::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
* @brief 関連アセットの検証を行う
*/
void UAnimNotify_AttachNiagara::ValidateAssociatedAssets()
{
	Super::ValidateAssociatedAssets();
}
#endif

/**
* @brief Niagaraエフェクトを生成する
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
* @return 生成されたNiagaraコンポーネント
*/
UNiagaraComponent* UAnimNotify_AttachNiagara::SpawnNiagara(USkeletalMeshComponent* MeshComp,
														   const UAnimSequenceBase* Animation) const
{
	if (!IsValid(MeshComp) || !IsValid(NiagaraSystem) || !IsValid(MeshComp->GetWorld()))
	{
		return nullptr;
	}
	bool IsAutoDestroy = DestroyAfterDeactivateTime > 0.f;

	UNiagaraComponent* Comp = nullptr;
	if (IsAttach)
	{
		Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem, // System
			MeshComp, // Parent
			AttachSocketName, // Socket
			LocationOffset, // 相対位置
			RotationOffset, // 相対回転
			EAttachLocation::KeepRelativeOffset, // 相対オフセットを保持
			IsAutoDestroy
		);
	}
	else
	{
		const FTransform MeshTransform = MeshComp->DoesSocketExist(AttachSocketName)
									 ? MeshComp->GetSocketTransform(AttachSocketName, RTS_World)
									 : MeshComp->GetComponentTransform();
		FRotator SpawnRot = MeshTransform.Rotator();
		SpawnRot += RotationOffset;
		Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MeshComp->GetWorld(),
			NiagaraSystem,
			MeshTransform.TransformPosition(LocationOffset),
			SpawnRot,
		FVector::One(),
			IsAutoDestroy);
	}
	return Comp;
}

/**
* @brief Niagaraエフェクトにフロートパラメータを設定する
* @param InNiagaraComponent 設定対象のNiagaraコンポーネント
*/
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

/**
* @brief Niagaraエフェクトにソケットの位置情報を設定する
* @param InNiagaraComponent 設定対象のNiagaraコンポーネント
* @param MeshComp スケルタルメッシュコンポーネント
*/
void UAnimNotify_AttachNiagara::SetSocketLocationToNiagara(UNiagaraComponent* InNiagaraComponent,
														   USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(InNiagaraComponent) || !IsValid(MeshComp))
	{
		return;
	}
	for (const auto& Param : InitialSocketLocationParameters)
	{
		const FName& SocketName = Param.Key;
		const FName& NiagaraVarName = Param.Value;
		if (SocketName.IsNone() || !MeshComp->DoesSocketExist(SocketName))
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[SetSocketLocationToNiagara] Socket '%s' was not found on mesh '%s' – "
					   "Niagara parameter '%s' will be skipped."),
				   *SocketName.ToString(),
				   *GetNameSafe(MeshComp),
				   *NiagaraVarName.ToString());
			continue;
		}
		const FTransform SocketTransform = MeshComp->GetSocketTransform(Param.Key);
		InNiagaraComponent->SetVariablePosition(Param.Value, SocketTransform.GetLocation());
	}
}

/**
* @brief Niagaraエフェクトの初期化を行う
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
*/
void UAnimNotify_AttachNiagara::InitializeNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (DelayActivateTime > .0f)
	{
		TWeakObjectPtr<USkeletalMeshComponent>  WeakMeshComp = MeshComp;
		TWeakObjectPtr<const UAnimSequenceBase> WeakAnimation = Animation;
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

/**
* @brief Niagaraエフェクトをアクティベートする
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
*/
void UAnimNotify_AttachNiagara::ActivateNiagara(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	UNiagaraComponent* NiagaraComponent = SpawnNiagara(MeshComp, Animation);
	if (!IsValid(NiagaraComponent))
	{
		return;
	}
	SetFloatParametersToNiagara(NiagaraComponent);
	SetSocketLocationToNiagara(NiagaraComponent, MeshComp);

	ScheduleDeactivate(NiagaraComponent, MeshComp, Animation);
	ScheduleDestroy(NiagaraComponent, MeshComp, Animation);
}

/**
* @brief Niagaraエフェクトの非アクティベートをスケジュールする
* @param NiagaraComponent 対象のNiagaraコンポーネント
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
*/
void UAnimNotify_AttachNiagara::ScheduleDeactivate(UNiagaraComponent* NiagaraComponent,
												   USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (!IsValid(MeshComp) || !IsValid(NiagaraComponent))
	{
		return;
	}

	if (DeactivateTime > .0f)
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
			}, DeactivateTime, false);
		}
	}
}

/**
* @brief Niagaraエフェクトの破棄をスケジュールする
* @param NiagaraComponent 対象のNiagaraコンポーネント
* @param MeshComp スケルタルメッシュコンポーネント
* @param Animation アニメーションシーケンス
*/
void UAnimNotify_AttachNiagara::ScheduleDestroy(UNiagaraComponent* NiagaraComponent, USkeletalMeshComponent* MeshComp,
												const UAnimSequenceBase* Animation)
{
	if (!IsValid(MeshComp) || !IsValid(NiagaraComponent))
	{
		return;
	}

	if (DestroyAfterDeactivateTime > .0f)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComponent = NiagaraComponent;
		if (auto World = MeshComp->GetWorld())
		{
			const float DestroyDelay = DeactivateTime + DestroyAfterDeactivateTime;
			FTimerHandle DestroyHandle;
			World->GetTimerManager().SetTimer(DestroyHandle, [WeakNiagaraComponent]()
			{
				if (auto* NiagaraComponentPtr = WeakNiagaraComponent.Get())
				{
					NiagaraComponentPtr->Deactivate();
					NiagaraComponentPtr->DestroyComponent();
				}
			}, DestroyDelay, false);
		}
	}
}