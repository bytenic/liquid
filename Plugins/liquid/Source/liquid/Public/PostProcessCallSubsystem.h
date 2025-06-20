// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "RuntimeAssetPtr.h"
#include "PostProcessCallSubsystem.generated.h"

/**
 * PostprocessMaterialの（float）パラメータをカーブで制御するための構造体
 */
USTRUCT(BlueprintType)
struct FPostProcessControlParams
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,meta=(ToolTip="操作するマテリアルパラメータ名"))
	FName MaterialParameterName = NAME_None;
	UPROPERTY(EditAnywhere,meta=(ToolTip="パラメータを操作するFloatカーブアセット"))
	TObjectPtr<UCurveFloat> NormalizedFloatCurve{};
};

/**
 * データテーブル(UPostProcessCallSubsystem::PostProcessTable)で定義されるポストプロセスエフェクト構成情報
 */
USTRUCT(BlueprintType)
struct FTransientPostProcessConfig : public FTableRowBase
{
	GENERATED_BODY()	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,meta=(ToolTip="適用するMaterial ※PostProcessMaterial以外を設定しないこと"))
	TSoftObjectPtr<UMaterialInstance> Material{nullptr};

	UPROPERTY(EditAnywhere,BlueprintReadOnly, meta=(ToolTip="適用順序のプライオリティ(低いほど先に実行されます)"))
	int32 Priority = 0;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, meta=(ToolTip="適用する時間"))
	float Duration = 2.0f;

	UPROPERTY(EditAnywhere,meta=(ToolTip="このポストプロセスのアルファ値の初期値(0で何もしない、1で完全適用)Weightをコントロールするカーブアセット"))
	float InitialWeight = 1.0f;
	
	UPROPERTY(EditAnywhere,meta=(ToolTip="このポストプロセスのアルファ値(0で何もしない、1で完全適用)Weightをコントロールするカーブアセット。Noneの場合はInitialWeightを使用します"))
	TObjectPtr<UCurveFloat> NormalizedWeightCurve{};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,meta=(ToolTip="操作するマテリアルパラメータ"))
	TArray<FPostProcessControlParams> ControlParameters;
};

/**
 * ポストプロセス実行タスクの進行状態
 */
enum class PostProcessTaskTickResult : uint8
{
	Progress,
	Finish
};

/**
 * 単一のポストプロセスエフェクトを実行・制御するタスク
 */
class FTransientPostProcessTask : public FGCObject
{
public:

	/**
	 * コンストラクタ
	 * * @param EffectID DataTable上のID
	 * @param ConfigPtr 使用する構成情報
	 * @param Owner 所有者（Subsystem）
	 */	
	explicit FTransientPostProcessTask(const FName& EffectID, const FTransientPostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner);
	/** GC参照の識別子名 */
	virtual FString GetReferencerName() const override;
	/** GC参照対象を追加 */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	/** タスクを初期化・有効化 */
	bool Activate(UMaterialInstance* OwnerMaterial);
	bool Activate(UMaterialInstance* OwnerMaterial, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction);
	/**
	 * タスクの更新処理
	 * @param CameraManager カメラマネージャへの参照
	 * @param DeltaTime 経過時間
	 */	
	PostProcessTaskTickResult Tick(APlayerCameraManager* CameraManager, float DeltaTime);

	const FName& GetEffectID() const{return EffectID;}
	const FTransientPostProcessConfig* GetConfig() const{return PostProcessConfig;}

	bool IsScheduleDeleteTask(float CurrentFrameDeltaTime) const;

private:
	bool CreateMaterialInstanceDynamic(UMaterialInstance* OwnerMaterial);
	void Cleanup();
	void InitializeOverrideSettings();
private:
	//memo: このふたつのポインタはUPostProcessCallSubsystemよりもこのクラスのライフサイクルが短いことと、DatatableをUPROPERTYで保持しているため生ポインタで保持している
	const FTransientPostProcessConfig* PostProcessConfig{nullptr};
	UPostProcessCallSubsystem* Owner{};

	//memo: このオブジェクトをGCオブジェクトとして保護
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstanceDynamic{nullptr};
	FPostProcessSettings OverrideSettings{};
	FName EffectID{}; //DataTable上のID
	float ElapsedTime = .0f; //秒
};

/**
 * データテーブルに基づいてポストプロセスエフェクトを適用するWorld Subsystem
 */
UCLASS(Config=Game)
class LIQUID_API UPostProcessCallSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * データテーブル上のIDに基づいてポストエフェクトを実行
	 * @param EffectID データテーブル内の行ID
	 */	
	UFUNCTION(BlueprintCallable,Category="PostProcess", meta=(ToolTip="データテーブル上のIDに基づいてポストエフェクトを呼び出します"))
	bool PlayTransientPostProcess(const FName& EffectID);
	bool PlayTransientPostProcess(const FName& EffectID, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction);

	bool IsPlayingTransientPostProcess(const FName& EffectID, const UWorld* InWorld) const;
	
private:
	/** エフェクトの適用開始 */
	bool BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config);
	bool BeginTransientPostProcess(const FName& EffectID, const FTransientPostProcessConfig* Config, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction);
	
	/** WorldのPostTick時に呼び出されるタスク更新関数 */
	void OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType,float DeltaTime);
	UMaterialInstance* GetLoadedMaterial(const FName& EffectID) const;

	void LoadPostProcessMaterialAsync(const FName& EffectID);
	static float GetEffectiveDeltaSeconds(const UWorld* InWorld);
private:
	
	/** ポストプロセス設定を格納したデータテーブル ※アセットのパスはTableAssetPathでハードコーディングされています*/
	UPROPERTY()
	TObjectPtr<UDataTable> PostProcessTable{nullptr};

	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInstance>> CachedMaterials;
	TArray<TUniquePtr<FTransientPostProcessTask>> TransientTasks;
	
	FDelegateHandle PostActorTickHandle;
	
	TQueue<FName> LoadMaterialQueue{};
	TSharedPtr<FStreamableHandle> CurrentLoadingHandle{};
	static constexpr TCHAR TableAssetPath[] = TEXT("/liquid/post_process/sample_table");
	static constexpr int32 TransientPostProcessCapacity = 16;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	TMap<FName, int32> LoadRetryCounts;
	static constexpr int32 MaxLoadRetryCount = 3;
#endif
};
