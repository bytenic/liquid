// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"

#include "Camera/CameraComponent.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelRead.h"
#include "Kismet/GameplayStatics.h"

static constexpr TCHAR PostProcessTableAssetPath[] = TEXT("/liquid/post_process/sample_table");
void UPostProcessCallSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostProcessTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, PostProcessTableAssetPath));
	
	if(!PostProcessTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Data Table Load Failed"));	
	}
}

void UPostProcessCallSubsystem::PlayPostEffect(const FName EffectID)
{
	if(!PostProcessTable)
	{
		return;
	}
	if(const FPostProcessConfig* Row = PostProcessTable->FindRow<FPostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
		BeginEffect(*Row);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Not Found ID: %s "), *EffectID.ToString());	
	}
}

void UPostProcessCallSubsystem::StopCurrentEffect()
{
	if(IsPlaying)
	{
		EndEffect();
	}
}

void UPostProcessCallSubsystem::BeginEffect(const FPostProcessConfig& Config)
{
	EndEffect();

	CurrentSettings = Config;
	ElapsedTime = .0f;
	IsPlaying = true;

	UMaterialInstance* Mat = Config.Material;
	if(!Mat)
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Material is nullptr "));
		IsPlaying = false;
		return;
	}
	CurrentPlayMID = UMaterialInstanceDynamic::Create(Mat,this);

	Blendable.Object = CurrentPlayMID;
	Blendable.Weight = 1.0f;
	//Blendable.
	OverrideSettings.WeightedBlendables.Array.Add(Blendable);
	//OverrideSettings.bOverride_w
	//OverrideSettings.bOverride_ColorSaturation = true;
	//OverrideSettings.ColorSaturation = FVector4(0.f, 0.f, 0.f, 1.f);
}

void UPostProcessCallSubsystem::EndEffect()
{
	if(!IsPlaying)
	{
		return;
	}
	OverrideSettings.WeightedBlendables.Array.Reset();
	CurrentPlayMID = nullptr;
	IsPlaying = false;
	auto PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(),0);
	//PCM->ClearCa
}

void UPostProcessCallSubsystem::Tick(float DeltaTime)
{
	if(!IsPlaying)
		return;
	UWorld* CurrentWorld = GetWorld();
	if(!CurrentWorld)
		return;
	
	//UE_LOG(LogTemp, Warning, TEXT("CachedPPBlends: %d"));

	ElapsedTime += DeltaTime;
	const float Duration = FMath::Max(CurrentSettings.Duration, KINDA_SMALL_NUMBER);

	float Alpha = FMath::Clamp(ElapsedTime / Duration, .0f, 1.0f);
	float Value = .0f;
	//if(CurrentSettings.para)
	
	Value = (Alpha < .5f) ? Alpha * 2.0f : (1.0 - Alpha) * 2.0f; //三角

	auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	//auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	PCM->AddCachedPPBlend(OverrideSettings, 1.1f, VTBlendOrder_Override);
	//APlayerCameraManager* CameraManager = PCM->PlayerCameraManager;
	if (PCM)
	{
		// 現在のViewTargetを取得
		AActor* ViewTarget = PCM->GetViewTarget();
		if (ViewTarget)
		{
			UE_LOG(LogTemp, Warning, TEXT("ViewTarget: %s"), *ViewTarget->GetName());

			// カメラコンポーネントを取得して確認する
			UCameraComponent* CameraComp = ViewTarget->FindComponentByClass<UCameraComponent>();
			
			if (CameraComp)
			{
				UE_LOG(LogTemp, Warning, TEXT("CameraComponent is attached to ViewTarget."));
				UE_LOG(LogTemp, Warning, TEXT("BlendWeight: %f"), CameraComp->PostProcessBlendWeight);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("No CameraComponent found in ViewTarget."));
			}
		}
	}
	
	//PCM->
	//if(CurrentPlayMID && CurrentSettings.ParameterName != NAME_None)
	//{
	//	CurrentPlayMID->SetScalarParameterValue()
	//}

	//CurrentWorld->GetWorldSettings()->SetOverridePostProcessSettings(OverrideSettings, 1.0f);
}
