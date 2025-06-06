// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"
#include "Kismet/GameplayStatics.h"

FPostProcessOverrideTask::FPostProcessOverrideTask(const FOverridePostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner)
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
	CameraManager->AddCachedPPBlend(OverrideSettings, PostProcessConfig->Weight, VTBlendOrder_Override);

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
		BeginEffect(Row);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Not Found ID: %s "), *EffectID.ToString());	
	}
}

void UPostProcessCallSubsystem::BeginEffect(const FOverridePostProcessConfig* Config)
{
	auto InitTask =	MakeUnique<FPostProcessOverrideTask>(Config, this);
	if (InitTask->Activate())
	{
		OverrideTasks.Emplace(MoveTemp(InitTask));
	}
}

void UPostProcessCallSubsystem::OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType, float DeltaTime)
{
	UWorld* CurrentWorld = GetWorld();
	if(!CurrentWorld)
		return;
	auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);

	TArray<int32,TInlineAllocator<16>> DeleteIndexArray{};
	const int32 NumTask = OverrideTasks.Num();
	if (NumTask)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Override Postprocess Task Size is Over Delete Index Array NumTask: %d"), NumTask);	
	}
	for (int32 Index = 0 ; Index < NumTask ; Index++)
	{
		if (OverrideTasks[Index]->Tick(PCM, DeltaTime) == PostProcessTaskTickResult::Finish)
		{
			DeleteIndexArray.Add(Index);
		}
	}
	for (const auto Index : DeleteIndexArray)
	{
		OverrideTasks.RemoveAt(Index);
	}
}
