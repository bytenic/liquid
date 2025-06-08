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
	if (!CreateMaterialInstanceDynamic())
	{
		return false;
	}
	InitializeOverrideSettings();
	return true;
}

bool FPostProcessOverrideTask::Activate(const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	if (!CreateMaterialInstanceDynamic())
	{
		return false;
	}
	InitFunction(MaterialInstanceDynamic);
	InitializeOverrideSettings();
	return true;
}

/**
 * カメラマネージャに対してポストプロセス設定を適用。
 * マテリアルパラメータをカーブに基づいて更新し、経過時間を考慮して進行状況を返す。
 *
 * @param CameraManager 対象のプレイヤーカメラマネージャ
 * @param DeltaTime 経過時間（秒）
 * @return タスクの進行状態
 */
PostProcessTaskTickResult FPostProcessOverrideTask::Tick(APlayerCameraManager* CameraManager, float DeltaTime)
{
	for (const auto& Parameters : PostProcessConfig->ControlParameters)
	{
		if (Parameters.MaterialParameterName ==  NAME_None)
		{
			continue;
		}
		if (Parameters.FloatCurve)
		{
			const float Value = Parameters.FloatCurve->GetFloatValue(ElapsedTime);
			MaterialInstanceDynamic->SetScalarParameterValue(Parameters.MaterialParameterName, Value);
		}
	}
	CameraManager->AddCachedPPBlend(OverrideSettings, PostProcessConfig->Weight, VTBlendOrder_Override);

	ElapsedTime += DeltaTime;
	return ElapsedTime >= PostProcessConfig->Duration ? PostProcessTaskTickResult::Finish : PostProcessTaskTickResult::Progress;
}

bool FPostProcessOverrideTask::CreateMaterialInstanceDynamic()
{
	if(!PostProcessConfig->Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPostProcessOverrideTask] Material is nullptr "));
		return false;
	}
	MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(PostProcessConfig->Material,Owner);
	return true;
}

void FPostProcessOverrideTask::InitializeOverrideSettings()
{
	FWeightedBlendable Blendable;
	Blendable.Object = MaterialInstanceDynamic;
	Blendable.Weight = 1.0f;
	OverrideSettings.WeightedBlendables.Array.Add(Blendable);
}

/**
 * サブシステムの初期化処理。
 * データテーブルを読み込み、更新処理をWorldPostActorTick に登録。
 */
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

/**
 * サブシステムの終了処理。デリゲート登録を解除。
 */
void UPostProcessCallSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
}

/**
 * データテーブルから指定IDの行を取得し、該当するエフェクトを実行
 *
 * @param EffectID データテーブル内の行ID
 */
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

void UPostProcessCallSubsystem::PlayPostEffect(const FName EffectID,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	if(!PostProcessTable)
	{
		return;
	}
	if(const FOverridePostProcessConfig* Row = PostProcessTable->FindRow<FOverridePostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
		BeginEffect(Row, InitFunction);
	}
}

/**
 * タスクを初期化・有効化し、実行中タスクリストに追加
 *
 * @param Config 実行するエフェクトの設定情報
 */
void UPostProcessCallSubsystem::BeginEffect(const FOverridePostProcessConfig* Config)
{
	auto InitTask =	MakeUnique<FPostProcessOverrideTask>(Config, this);
	if (InitTask->Activate())
	{
		OverrideTasks.Emplace(MoveTemp(InitTask));
	}
}

void UPostProcessCallSubsystem::BeginEffect(const FOverridePostProcessConfig* Config,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	auto InitTask =	MakeUnique<FPostProcessOverrideTask>(Config, this);
	if (InitTask->Activate(InitFunction))
	{
		OverrideTasks.Emplace(MoveTemp(InitTask));
	}
}

/**
 * WorldのPostActorTickイベントで呼び出される。全てのタスクを更新。
 * 終了済みのタスクは削除。
 *
 * @param InWorld World参照
 * @param InType Tickタイプ
 * @param DeltaTime 経過時間
 */
void UPostProcessCallSubsystem::OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType, float DeltaTime)
{
	UWorld* CurrentWorld = GetWorld();
	if(!CurrentWorld)
	{
		return;
	}
	auto PCM = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	if (!PCM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] CameraManager is nullptr"));
		return;
	}
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
