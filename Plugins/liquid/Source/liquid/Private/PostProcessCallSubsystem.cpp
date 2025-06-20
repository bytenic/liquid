// Fill out your copyright notice in the Description page of Project Settings.


#include "PostProcessCallSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

FTransientPostProcessTask::FTransientPostProcessTask(const FName& EffectID,const FTransientPostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner)
	: PostProcessConfig(ConfigPtr), Owner(Owner), EffectID(EffectID)
{
	check(Owner);
}

FString FTransientPostProcessTask::GetReferencerName() const
{
	return TEXT("TransientPostProcessTask");
}

void FTransientPostProcessTask::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MaterialInstanceDynamic);
}

bool FTransientPostProcessTask::Activate(UMaterialInstance* OwnerMaterial)
{
	if (!CreateMaterialInstanceDynamic(OwnerMaterial))
	{
		return false;
	}
	InitializeOverrideSettings();
	return true;
}

bool FTransientPostProcessTask::Activate(UMaterialInstance* OwnerMaterial, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	if (!CreateMaterialInstanceDynamic(OwnerMaterial))
	{
		return false;
	}
	InitFunction(MaterialInstanceDynamic);
	InitializeOverrideSettings();
	return true;
}

/**
 * @details
 * - 経過時間を更新し、NormalizedElapsedTime(0‑1) を算出。
 * - ControlParameters で指定された MID のスカラーをカーブで更新。
 * - AddCachedPPBlend() で PostProcess をカメラへ適用。
 * - 寿命(ElapsedTime >= Duration) を迎えたら Cleanup() し Finish を返す。
 */
PostProcessTaskTickResult FTransientPostProcessTask::Tick(APlayerCameraManager* CameraManager, float DeltaTime)
{
	ElapsedTime += DeltaTime;
	float NormalizedElapsedTime = ElapsedTime / PostProcessConfig->Duration;
	NormalizedElapsedTime = FMath::Clamp(NormalizedElapsedTime, 0.0f, 1.0f);
	for (const auto& Parameters : PostProcessConfig->ControlParameters)
	{
		if (Parameters.MaterialParameterName ==  NAME_None)
		{
			continue;
		}
		if (Parameters.NormalizedFloatCurve)
		{
			const float Value = Parameters.NormalizedFloatCurve->GetFloatValue(NormalizedElapsedTime);
			MaterialInstanceDynamic->SetScalarParameterValue(Parameters.MaterialParameterName, Value);
		}
	}
	float CurrentWeight = PostProcessConfig->InitialWeight;
	if (PostProcessConfig->NormalizedWeightCurve)
	{
		CurrentWeight = PostProcessConfig->NormalizedWeightCurve->GetFloatValue(NormalizedElapsedTime);
	}
	CurrentWeight = FMath::Clamp(CurrentWeight, 0.0f, 1.0f);
	CameraManager->AddCachedPPBlend(OverrideSettings, CurrentWeight, VTBlendOrder_Override);
	
	if ( ElapsedTime >= PostProcessConfig->Duration)
	{
		Cleanup();
		return PostProcessTaskTickResult::Finish;
	}
	return PostProcessTaskTickResult::Progress;

}

bool FTransientPostProcessTask::IsScheduleDeleteTask(float CurrentFrameDeltaTime) const
{
	return ElapsedTime + CurrentFrameDeltaTime >= PostProcessConfig->Duration;
}

bool FTransientPostProcessTask::CreateMaterialInstanceDynamic(UMaterialInstance* OwnerMaterial)
{
	MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(OwnerMaterial,Owner);
	if (!MaterialInstanceDynamic)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPostProcessOverrideTask] Failed Create Material Instance Dynamic"));
		return false;
	}
	return true;
}

void FTransientPostProcessTask::Cleanup()
{
	OverrideSettings.WeightedBlendables.Array.Empty();
	// MID を即座に GC 対象へ
	if (MaterialInstanceDynamic)
	{
		MaterialInstanceDynamic->MarkAsGarbage();
		MaterialInstanceDynamic = nullptr;
	}
}

void FTransientPostProcessTask::InitializeOverrideSettings()
{
	FWeightedBlendable Blendable;
	Blendable.Object = MaterialInstanceDynamic;
	Blendable.Weight = 1.0f;
	OverrideSettings.WeightedBlendables.Array.Add(Blendable);
}

/**
 * @brief サブシステム初期化。
 * - Datatable をロード
 * - PostActorTick デリゲート登録
 * - Datatable 行毎にマテリアルを非同期ロード開始
 */
void UPostProcessCallSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostProcessTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TableAssetPath));
	TransientTasks.Reserve(TransientPostProcessCapacity);
	if(!PostProcessTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Data Table Load Failed"));
		return;
	}
	
	PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(
		this, &UPostProcessCallSubsystem::OnWorldPostActorTick);

	TArray<FName> RowNames = PostProcessTable->GetRowNames();
	CachedMaterials.Reserve(RowNames.Num());
	for (auto& Name : RowNames)
	{
		LoadMaterialQueue.Enqueue(MoveTemp(Name));
	}
	FName FirstLoadID;
	LoadMaterialQueue.Dequeue(FirstLoadID);
	LoadPostProcessMaterialAsync(FirstLoadID);
}

/**
 * @brief Datatable 行に紐付くマテリアルを非同期ロード。(逐次処理)
 * @param EffectID 行ID
 */
void UPostProcessCallSubsystem::LoadPostProcessMaterialAsync(const FName& EffectID)
{
	const FTransientPostProcessConfig* Row = PostProcessTable->FindRow<FTransientPostProcessConfig>(EffectID, TEXT("UPostProcessCallSubsystem::Initialize"));
	if (!Row || Row->Material.IsNull())
	{
		UE_LOG(LogTemp, Error,
		TEXT("[UPostProcessCallSubsystem::Initialize] Row %s has null Material"), *EffectID.ToString());
		return;;
	}

	FStreamableManager& Manager = UAssetManager::GetStreamableManager();
	CurrentLoadingHandle = Manager.RequestAsyncLoad(
	Row->Material.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([this,Row, EffectID]()
		{
			UMaterialInstance* LoadedMaterial = Row->Material.Get();
			bool IsSuccessful = Row->Material.IsValid() && LoadedMaterial != nullptr;
			if (!IsSuccessful)
			{
				int32& RetryCount = LoadRetryCounts.FindOrAdd(EffectID);
				if (++RetryCount <= MaxLoadRetryCount)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[PostProcessCallSubsystem] Retry %d / %d : %s"),
						RetryCount, MaxLoadRetryCount, *EffectID.ToString());
					LoadPostProcessMaterialAsync(EffectID);
					return;
				}
			}
			if (LoadedMaterial)
			{
				CachedMaterials.Add(EffectID, LoadedMaterial);
				UE_LOG(LogTemp, Log,
				   TEXT("[UPostProcessCallSubsystem::Initialize] Loaded PostProcess Material for %s"), *EffectID.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Error,
				   TEXT("[UPostProcessCallSubsystem::Initialize] Failed to load PostProcess Material for %s"),
				   *EffectID.ToString());
			}

			if (!LoadMaterialQueue.IsEmpty())
			{
				FName NextID;
				LoadMaterialQueue.Dequeue(NextID);
				LoadPostProcessMaterialAsync(NextID);
			}
		}));
}

float UPostProcessCallSubsystem::GetEffectiveDeltaSeconds(const UWorld* InWorld)
{
	return (InWorld != nullptr)? InWorld->GetDeltaSeconds() : FApp::GetDeltaTime();   // World が分かるならその Δt: FApp::GetDeltaTime();
}

/**
 * サブシステムの終了処理。デリゲート登録を解除。
 */
void UPostProcessCallSubsystem::Deinitialize()
{
	if (CurrentLoadingHandle.IsValid())
	{
		CurrentLoadingHandle->CancelHandle();
	}
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
}

/**
 * データテーブルから指定IDの行を取得し、該当するエフェクトを実行
 *
 * @param EffectID データテーブル内の行ID
 */
bool UPostProcessCallSubsystem::PlayTransientPostProcess(const FName& EffectID)
{
	if(!PostProcessTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] PostProcessTable is nullptr"));
		return false;
	}
	if(const FTransientPostProcessConfig* Row = PostProcessTable->FindRow<FTransientPostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
	
		if (Row->Duration <= .0f)
		{
			UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Duration is 0 EffectID: %s "), *EffectID.ToString());
			return false;
		}
		return BeginTransientPostProcess(EffectID, Row);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Not Found ID: %s "), *EffectID.ToString());	
	}
	return false;
}

bool UPostProcessCallSubsystem::PlayTransientPostProcess(const FName& EffectID,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	if(!PostProcessTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] PostProcessTable is nullptr"));
		return false;
	}
	if(const FTransientPostProcessConfig* Row = PostProcessTable->FindRow<FTransientPostProcessConfig>(EffectID, TEXT("PostProcessCallSubsystem")))
	{
		if (Row->Duration <= .0f)
		{
			UE_LOG(LogTemp, Error, TEXT("[UPostProcessCallSubsystem] Duration is 0 EffectID: %s "), *EffectID.ToString());
			return false;
		}
		return BeginTransientPostProcess(EffectID, Row, InitFunction);
	}
	return false;
}

bool UPostProcessCallSubsystem::IsPlayingTransientPostProcess(const FName& EffectID, const UWorld* InWorld) const
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
		float Delta = GetEffectiveDeltaSeconds(InWorld);
		return !(*ExistTask)->IsScheduleDeleteTask(Delta);
	}
	return false;
}

/**
 * タスクを初期化・有効化し、実行中タスクリストに追加
 *
 * @param EffectID DataTable上のID
 * @param Config 実行するエフェクトの設定情報
 */
bool UPostProcessCallSubsystem::BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config)
{
	UMaterialInstance* LoadedMat = GetLoadedMaterial(EffectID);
	if (!LoadedMat)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[UPostProcessCallSubsystem::BeginTransientPostProcess] Material for %s is not loaded yet. PostProcess call aborted."),
			*EffectID.ToString());
		return false;
	}
	auto InitTask =	MakeUnique<FTransientPostProcessTask>(EffectID, Config, this);
	if (InitTask->Activate(LoadedMat))
	{
		TransientTasks.Emplace(MoveTemp(InitTask));
		//memo: 各TaskのTickは降順に実行されるのでここでも降順に実行することで結果的に昇順のタスク実行になるようにする
		TransientTasks.Sort([](const TUniquePtr<FTransientPostProcessTask>& A, const TUniquePtr<FTransientPostProcessTask>& B)
		{
			return A->GetConfig()->Priority > B->GetConfig()->Priority; 
		});
		return true;
	}
	return false;
}

bool UPostProcessCallSubsystem::BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config,
	const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction)
{
	UMaterialInstance* LoadedMat = GetLoadedMaterial(EffectID);
	if (!LoadedMat)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[UPostProcessCallSubsystem::BeginTransientPostProcess] Material for %s is not loaded yet. PostProcess call aborted."),
			*EffectID.ToString());
		return false;
	}
	
	auto InitTask =	MakeUnique<FTransientPostProcessTask>(EffectID, Config, this);
	if (InitTask->Activate(LoadedMat, InitFunction))
	{
		TransientTasks.Emplace(MoveTemp(InitTask));
		//memo: 各TaskのTickは降順に実行されるのでここでも降順に実行することで結果的に昇順のタスク実行になるようにする
		TransientTasks.Sort([](const TUniquePtr<FTransientPostProcessTask>& A, const TUniquePtr<FTransientPostProcessTask>& B)
		{
			return A->GetConfig()->Priority > B->GetConfig()->Priority; 
		});
		return true;
	}
	return false;
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
	auto PlayerCameraManager = UGameplayStatics::GetPlayerCameraManager(CurrentWorld,0);
	if (!PlayerCameraManager)
	{
		//Editorでもここに来るのでVerboseにする
		UE_LOG(LogTemp, Verbose, TEXT("[UPostProcessCallSubsystem] CameraManager is nullptr"));
		return;
	}
	const int32 NumTask = TransientTasks.Num();
	if (NumTask >= TransientPostProcessCapacity)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPostProcessCallSubsystem] Transient Postprocess Task Size is Over Delete Index Array NumTask: %d"), NumTask);	
	}
	//memo: 独立したタスク削除ループを行わないようにするために降順でTickをまわすTransientTasksは降順にソートされているため結果的に昇順に実行される
	for (int32 Index = NumTask -1 ; Index >= 0 ; --Index)
	{
		//UE_LOG(LogTemp, Log, TEXT("[UPostProcessCallSubsystem] Tick %s"),*TransientTasks[Index]->GetEffectID().ToString());
		if (TransientTasks[Index]->Tick(PlayerCameraManager, DeltaTime) == PostProcessTaskTickResult::Finish)
		{
			//UE_LOG(LogTemp, Log, TEXT("[UPostProcessCallSubsystem] Finish %s"),*TransientTasks[Index]->GetEffectID().ToString());
			TransientTasks.RemoveAt(Index);
		}
	}
}

UMaterialInstance* UPostProcessCallSubsystem::GetLoadedMaterial(const FName& EffectID) const
{
	const TObjectPtr<UMaterialInstance>* Found = CachedMaterials.Find(EffectID);
	return Found ? Found->Get() : nullptr;
}


