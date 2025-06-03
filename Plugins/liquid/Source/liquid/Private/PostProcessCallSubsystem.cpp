// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"

#include "Kismet/GameplayStatics.h"

FPostProcessOverrideTask::FPostProcessOverrideTask(FOverridePostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner)
	: PostProcessConfig(ConfigPtr), Owner(Owner)
{
	
}

FString FPostProcessOverrideTask::GetReferencerName() const
{
	return TEXT("PostProcessOverrideTask");
}

void FPostProcessOverrideTask::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MaterialInstanceDynamic);
}

bool FPostProcessOverrideTask::Activate()
{
	if(!PostProcessConfig->Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPostProcessOverrideTask] Material is nullptr "));
		return false;
	}
	MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(PostProcessConfig->Material,Owner);
	FWeightedBlendable Blendable;
	Blendable.Object = MaterialInstanceDynamic;
	Blendable.Weight = 1.0f;
	OverrideSettings.WeightedBlendables.Array.Add(Blendable);
	return true;
}

PostProcessTaskTickResult FPostProcessOverrideTask::Tick(APlayerCameraManager* CameraManager, float DeltaTime)
{
	CameraManager->AddCachedPPBlend(OverrideSettings, 1.f, VTBlendOrder_Override);

	for (const auto& Parameters : PostProcessConfig->ControlParameters)
	{
		if (Parameters.MaterialParameterName ==  NAME_None)
		{
			continue;
		}
		const float Value = Parameters.FloatCurve->GetFloatValue(ElapsedTime);
		MaterialInstanceDynamic->SetScalarParameterValue(Parameters.MaterialParameterName, Value);
	}
	
	ElapsedTime += DeltaTime;
	return ElapsedTime >= PostProcessConfig->Duration ? PostProcessTaskTickResult::Finish : PostProcessTaskTickResult::Progress;
}

void UPostProcessCallSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostProcessTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TableAssetPath));
	
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
	if(const FOverridePostProcessConfig* Row = PostProcessTable->FindRow<FOverridePostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
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

void UPostProcessCallSubsystem::BeginEffect(const FOverridePostProcessConfig& Config)
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
	FWeightedBlendable Blendable;
	Blendable.Object = CurrentPlayMID;
	Blendable.Weight = 1.0f;
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
	ElapsedTime += DeltaTime;
	if (CurrentPlayMID)
	{
		for (const auto& ParameterSet : CurrentSettings.ControlParameters)
		{
			if (ParameterSet.MaterialParameterName == NAME_None)
				continue;
			const float Value = ParameterSet.FloatCurve->GetFloatValue(ElapsedTime);
			CurrentPlayMID->SetScalarParameterValue(ParameterSet.MaterialParameterName, Value);
		}
	}
	auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	PCM->AddCachedPPBlend(OverrideSettings, 1.f, VTBlendOrder_Override);
}
