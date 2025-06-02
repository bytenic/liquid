// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"

#include "Camera/CameraComponent.h"
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
	PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(
		this, &UPostProcessCallSubsystem::OnWorldPostActorTick);
}

void UPostProcessCallSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
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
}

void UPostProcessCallSubsystem::OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType, float DeltaTime)
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
	
	Value = (Alpha < .5f) ? Alpha * 2.0f : (1.0 - Alpha) * 2.0f; //三角
	auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	PCM->AddCachedPPBlend(OverrideSettings, 1.f, VTBlendOrder_Override);
	//PCM->
	//if(CurrentPlayMID && CurrentSettings.ParameterName != NAME_None)
	//{
	//	CurrentPlayMID->SetScalarParameterValue()
	//}

	//CurrentWorld->GetWorldSettings()->SetOverridePostProcessSettings(OverrideSettings, 1.0f);
}
