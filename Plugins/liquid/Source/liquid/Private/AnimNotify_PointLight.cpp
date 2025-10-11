// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_PointLight.h"
#include "TransientPointLightComponent.h"


void UAnimNotify_PointLight::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}
	UTransientPointLightComponent* Comp = SpawnPointLight(MeshComp, Animation);
	if (!Comp)
	{
		UE_LOG(LogTemp, Error, TEXT("[UAnimNotify_PointLight::Notify] Failed Spawn Point Light"));
	}

}

UTransientPointLightComponent* UAnimNotify_PointLight::SpawnPointLight(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation)
{
	if (!MeshComp) { return nullptr; }

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) { return nullptr; }

	// ランタイム生成（Transientフラグ推奨）
	UTransientPointLightComponent* Comp =
		NewObject<UTransientPointLightComponent>(Owner, UTransientPointLightComponent::StaticClass(), NAME_None, RF_Transient);
	if (!Comp) { return nullptr; }

	// パラメータ適用（強度・色・寿命・カーブ等）
	Comp->SetParameters(Parameters);

	// アタッチ設定
	if (bIsAttach)
	{
		const bool HasSocket = (SocketName.IsNone() ? false : MeshComp->DoesSocketExist(SocketName));
		if (!HasSocket && !SocketName.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PointLightNotify] Socket '%s' not found. Attaching to Mesh root instead."), *SocketName.ToString());
		}
		Comp->SetupAttachment(MeshComp, SocketName);
		// 相対オフセットを適用（ソケット基準）
		Comp->SetRelativeLocation(LocationOffset);
	}
	else
	{
		// Ownerルートへぶら下げる（ワールド位置は後で設定）
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Comp->SetupAttachment(Root);
		}
	}

	// 登録（これ以降はトランスフォームが有効）
	Comp->RegisterComponent();

	// 非アタッチ時は「メッシュ座標系でのオフセット」をワールドに変換して配置
	if (!(bIsAttach && MeshComp))
	{
		const FTransform MeshXf =MeshComp->DoesSocketExist(SocketName) ? MeshComp->GetSocketTransform(SocketName, RTS_World): MeshComp->GetComponentTransform();
		const FVector WorldLoc = MeshXf.TransformPosition(LocationOffset);
		Comp->SetWorldLocation(WorldLoc);
	}

	// 起動
	Comp->Activate(true);
	
	return Comp;
}
