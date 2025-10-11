// Fill out your copyright notice in the Description page of Project Settings.


#include "TransientPointLightComponent.h"

#include "Curves/CurveLinearColor.h"
#include "Runtime/CrashReportCore/Public/CrashReportAnalytics.h"

UTransientPointLightComponent::UTransientPointLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTransientPointLightComponent::OnRegister()
{
	Super::OnRegister();
	//UE_LOG(LogTemp, Log, TEXT("OnRegister"));
	if (!bIsInitialized)
	{
		InitParameters();
	}
}

void UTransientPointLightComponent::OnUnregister()
{
	//Destroy
	Super::OnUnregister();
}

void UTransientPointLightComponent::BeginPlay()
{
	Super::BeginPlay();
	//UE_LOG(LogTemp, Log, TEXT("BeginPlay"));
	if (!bIsInitialized)
	{
		InitParameters();
	}
	
}

void UTransientPointLightComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	CurrentDuration += DeltaTime;
	// DestroyTimeが無効なら即破棄
	if (Parameters.DestroyTime <= KINDA_SMALL_NUMBER)
	{
		DestroyComponent();
		return;
	}

	// 0-1 の正規化時間
	const float NormalizedTime = FMath::Clamp(CurrentDuration / Parameters.DestroyTime, 0.0f, 1.0f);

	// 強度更新（ベース×カーブ×スケール）
	
	if (Parameters.IntensityCurve)
	{
		float IntensityScale = Parameters.IntensityCurve->GetFloatValue(NormalizedTime) * Parameters.IntensityCurveScale;
		SetIntensity(Parameters.Intensity * IntensityScale);
	}
	

	// 色更新（カーブがあればカーブ、なければベース色）
	if (Parameters.ColorCurve)
	{
		const FLinearColor CurColor = Parameters.ColorCurve->GetLinearColorValue(NormalizedTime);
		SetLightColor(CurColor);
	}
	//UE_LOG(LogTemp, Log, TEXT("Tick"));
	// 寿命到達で自動破棄
	if (CurrentDuration >= Parameters.DestroyTime)
	{
		//UE_LOG(LogTemp, Log, TEXT("DestroyComponent"));
		DestroyComponent();
	}
}

void UTransientPointLightComponent::SetParameters(const FTransientPointLightDesc& Desc)
{
	Parameters = Desc;
}

void UTransientPointLightComponent::InitParameters()
{
	CurrentDuration = 0.0f;
	SetIntensity(Parameters.Intensity);
	SetLightColor(Parameters.LightColor);
	SetVisibility(true, true);
	SetComponentTickEnabled(true);
	bIsInitialized = true;
}
