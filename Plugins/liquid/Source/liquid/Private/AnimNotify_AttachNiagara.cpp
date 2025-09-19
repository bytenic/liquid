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
	ActivateNiagara(MeshComp, Animation);
}

/**
* @brief オブジェクトが破棄される時に呼び出される
*/
void UAnimNotify_AttachNiagara::BeginDestroy()
{
	Super::BeginDestroy();
}

FString UAnimNotify_AttachNiagara::GetNotifyName_Implementation() const
{
	if (NiagaraSystem)
	{
		return NiagaraSystem->GetName();
	}
	return Super::GetNotifyName_Implementation();
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
	
	UNiagaraComponent* Comp = nullptr;
	if (bIsAttach)
	{
		Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem, // System
			MeshComp, // Parent
			AttachSocketName, // Socket
			LocationOffset, // 相対位置
			RotationOffset, // 相対回転
			EAttachLocation::KeepRelativeOffset, // 相対オフセットを保持
			true
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
			true);
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
	for (const auto& Param : SocketLocationParameters)
	{
		const FName& SocketName = Param.Key;
		const FName& NiagaraVarName = Param.Value.SocketName;
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
		const FTransform SocketTransform = (Param.Value.bIsLocalSpace) ?
		SocketTransform.GetRelativeTransform(InNiagaraComponent->GetComponentTransform())
		:
		MeshComp->GetSocketTransform(Param.Key);
		 
		InNiagaraComponent->SetVariablePosition(NiagaraVarName, SocketTransform.GetLocation());
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

	if (IsNeedUpdateSocketLocation())
	{
		ScheduleUpdateSocketLocation(NiagaraComponent, MeshComp, Animation);
	}
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

	if (DeactivateTime > .0f && DeactivateTime < DestroyTime)
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
					NiagaraComponentPtr->DestroyComponent();
				}
			}, DestroyTime, false);
		}
	}
}

bool UAnimNotify_AttachNiagara::IsNeedUpdateSocketLocation() const
{
	for (const auto& Param : SocketLocationParameters)
	{
		if (Param.Value.bIsRequiareUpdate)
		{
			return true;
		}
	}
	return false;
	
}

void UAnimNotify_AttachNiagara::ScheduleUpdateSocketLocation(UNiagaraComponent* NiagaraComponent,
	USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation)
{
	if (DestroyTime <= .0f)
	{
		return;
	}
	if (!IsValid(MeshComp) || !IsValid(NiagaraComponent))
	{
		return;
	}
	auto World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}
	TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComponent = NiagaraComponent;
	TWeakObjectPtr<USkeletalMeshComponent> WeakMeshComp = MeshComp;
	TWeakObjectPtr<UAnimNotify_AttachNiagara> WeakSelf = this;
	TWeakObjectPtr<UWorld> WorldWeakPtr = World;

	FTimerHandle TickTimerHandle;
	FTimerDelegate TickDelegate = FTimerDelegate::CreateLambda([TickTimerHandle, WeakSelf, WeakMeshComp, WeakNiagaraComponent, WorldWeakPtr]() mutable
	{
		UAnimNotify_AttachNiagara* ThisPtr = WeakSelf.Get();
		UNiagaraComponent* NiagaraComponent = WeakNiagaraComponent.Get();
		USkeletalMeshComponent* MeshComp = WeakMeshComp.Get();
		if (!ThisPtr || !NiagaraComponent || !MeshComp)
		{
			if (UWorld* CurrentWorld = WorldWeakPtr.Get())
			{
				CurrentWorld->GetTimerManager().ClearTimer(TickTimerHandle);
			}
			return;
		}
		if (IsValid(ThisPtr) && IsValid(NiagaraComponent) && IsValid(MeshComp))
		{
			ThisPtr->SetSocketLocationToNiagara(NiagaraComponent, MeshComp);
		}
	});

	static constexpr float TickInterval = 1.0f /60.0f;
	World->GetTimerManager().SetTimer(TickTimerHandle, TickDelegate, TickInterval, true);
	FTimerHandle StopHandle;
	FTimerDelegate StopDelegate = FTimerDelegate::CreateLambda([TickTimerHandle, WorldWeakPtr]() mutable
	{
		if (UWorld* CurrentWorld = WorldWeakPtr.Get())
		{
			CurrentWorld->GetTimerManager().ClearTimer(TickTimerHandle);
		}
	});
	World->GetTimerManager().SetTimer(StopHandle, StopDelegate, DestroyTime, false);	
}
