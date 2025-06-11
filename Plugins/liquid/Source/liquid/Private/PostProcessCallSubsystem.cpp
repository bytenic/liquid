// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"
#include "Kismet/GameplayStatics.h"

FTransientPostProcessTask::FTransientPostProcessTask(const FName& EffectID,const FTransientPostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner)
	: PostProcessConfig(ConfigPtr), Owner(Owner), EffectID(EffectID)
{
	
}

FString FTransientPostProcessTask::GetReferencerName() const
{
	return TEXT("PostProcessOverrideTask");
}

void FTransientPostProcessTask::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MaterialInstanceDynamic);
}

bool FTransientPostProcessTask::Activate()
{
	if (!CreateMaterialInstanceDynamic())
	{
		return false;
	}
	InitializeOverrideSettings();
	return true;
}

bool FTransientPostProcessTask::Activate(const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
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
PostProcessTaskTickResult FTransientPostProcessTask::Tick(APlayerCameraManager* CameraManager, float DeltaTime)
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

bool FTransientPostProcessTask::IsScheduleDeleteTask(float CurrentFrameDeltaTime) const
{
	return ElapsedTime + CurrentFrameDeltaTime >= PostProcessConfig->Duration;
}

bool FTransientPostProcessTask::CreateMaterialInstanceDynamic()
{
	if(!PostProcessConfig->Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPostProcessOverrideTask] Material is nullptr "));
		return false;
	}
	MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(PostProcessConfig->Material,Owner);
	return true;
}

void FTransientPostProcessTask::InitializeOverrideSettings()
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
void UPostProcessCallSubsystem::PlayTransientPostProcess(const FName& EffectID)
{
	if(!PostProcessTable)
	{
		return;
	}
	if(const FTransientPostProcessConfig* Row = PostProcessTable->FindRow<FTransientPostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
		BeginTransientPostProcess(EffectID, Row);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Not Found ID: %s "), *EffectID.ToString());	
	}
}

void UPostProcessCallSubsystem::PlayTransientPostProcess(const FName& EffectID,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	if(!PostProcessTable)
	{
		return;
	}
	if(const FTransientPostProcessConfig* Row = PostProcessTable->FindRow<FTransientPostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
		BeginTransientPostProcess(EffectID, Row, InitFunction);
	}
}

bool UPostProcessCallSubsystem::IsPlayingTransientPostProcess(const FName& EffectID, bool IsNotPostActorTick) const
{
	auto ExistTask = TransientTasks.FindByPredicate(
		[&EffectID](const TUniquePtr<FTransientPostProcessTask>& Task)
		{
			return Task->GetEffectID() == EffectID;
		});
	if (!ExistTask)
	{
		return false;
	}
	if (ExistTask->IsValid())
	{
		if (IsNotPostActorTick)
		{
			float Delta = UGameplayStatics::GetWorldDeltaSeconds(GetWorld());
			return (*ExistTask)->IsScheduleDeleteTask(Delta);
		}
		return true;
	}
	
	return false;
}

/**
 * タスクを初期化・有効化し、実行中タスクリストに追加
 *
 * @param EffectID DataTable上のID
 * @param Config 実行するエフェクトの設定情報
 */
void UPostProcessCallSubsystem::BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config)
{
	auto InitTask =	MakeUnique<FTransientPostProcessTask>(EffectID, Config, this);
	if (InitTask->Activate())
	{
		TransientTasks.Emplace(MoveTemp(InitTask));
		//memo: 各TaskのTickは降順に実行されるのでここでも降順に実行することで結果的に昇順のタスク実行になるようにする
		TransientTasks.Sort([](const TUniquePtr<FTransientPostProcessTask>& A, const TUniquePtr<FTransientPostProcessTask>& B)
		{
			return A->GetConfig()->Priority > B->GetConfig()->Priority; 
		});
	}
}

void UPostProcessCallSubsystem::BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	auto InitTask =	MakeUnique<FTransientPostProcessTask>(EffectID, Config, this);
	if (InitTask->Activate(InitFunction))
	{
		TransientTasks.Emplace(MoveTemp(InitTask));
		//memo: 各TaskのTickは降順に実行されるのでここでも降順に実行することで結果的に昇順のタスク実行になるようにする
		TransientTasks.Sort([](const TUniquePtr<FTransientPostProcessTask>& A, const TUniquePtr<FTransientPostProcessTask>& B)
		{
			return A->GetConfig()->Priority > B->GetConfig()->Priority; 
		});
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
		//Editorでもここに来るのでVerboseにする
		UE_LOG(LogTemp, Verbose, TEXT("[UPostProcessCallSubsystem] CameraManager is nullptr"));
		return;
	}
	const int32 NumTask = TransientTasks.Num();
	if (NumTask >= 16)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Override Postprocess Task Size is Over Delete Index Array NumTask: %d"), NumTask);	
	}
	//memo: 独立したタスク削除ループを行わないようにするために降順でTickをまわす。OverrideTasksは降順にソートされているため結果的に昇順に実行される
	for (int32 Index = NumTask -1 ; Index >= 0 ; --Index)
	{
		//UE_LOG(LogTemp, Log, TEXT("[UPostProcessCallSubsystem] Tick %s"),*OverrideTasks[Index]->GetEffectID().ToString());
		if (TransientTasks[Index]->Tick(PCM, DeltaTime) == PostProcessTaskTickResult::Finish)
		{
			//UE_LOG(LogTemp, Log, TEXT("[UPostProcessCallSubsystem] Finish %s"),*OverrideTasks[Index]->GetEffectID().ToString());
			TransientTasks.RemoveAt(Index);
		}
	}
}
