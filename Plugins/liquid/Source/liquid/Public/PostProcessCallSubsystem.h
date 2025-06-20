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
	/**
	 * Weight(0‑1) を時間で制御する FloatCurve。None の場合 InitialWeight を使用。
	 * 0 で無効、1 でフル適用。
	 */	
	UPROPERTY(EditAnywhere,meta=(ToolTip="このポストプロセスのアルファ値(0で何もしない、1で完全適用)Weightをコントロールするカーブアセット。Noneの場合はInitialWeightを使用します"))
	TObjectPtr<UCurveFloat> NormalizedWeightCurve{};
	/** カーブ制御により変更するスカラーパラメータリスト */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,meta=(ToolTip="操作するマテリアルパラメータ"))
	TArray<FPostProcessControlParams> ControlParameters;
};

/**
 * @brief ポストプロセスタスクの Tick 戻り値。
 */
enum class PostProcessTaskTickResult : uint8
{
	Progress,
	Finish
};

/**
 * @brief 単一のポストプロセスエフェクトを実行・制御する GC 対応タスク。
 *
 * FGCObject を継承し、内部で生成した UMaterialInstanceDynamic を GC 参照で保護する。
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
	/**
	 * @brief タスクを初期化して有効化。
	 * @param OwnerMaterial 事前ロード済みのベースマテリアル
	 * @return 成功した場合 true
	 */
	bool Activate(UMaterialInstance* OwnerMaterial);
	/**
	 * @brief 初期化用ラムダを受け取りつつタスクを有効化。
	 * @param OwnerMaterial ベースマテリアル
	 * @param InitFunction  M.I.D. に対し初期値を設定するユーザコールバック
	 * @return 成功した場合 true
	 */
	bool Activate(UMaterialInstance* OwnerMaterial, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction);
	/**
	 * @brief 1フレーム分更新し、ポストプロセスをカメラに反映する。
	 * @param CameraManager 対象の APlayerCameraManager
	 * @param DeltaTime     経過時間[秒]
	 * @return 進行状態 (Progress / Finish)
	 */
	PostProcessTaskTickResult Tick(APlayerCameraManager* CameraManager, float DeltaTime);
	/** @return データテーブル上の EffectID */
	const FName& GetEffectID() const{return EffectID;}
	/** @return タスクに紐付く構成情報 */
	const FTransientPostProcessConfig* GetConfig() const{return PostProcessConfig;}
	/**
	 * @brief フレーム終了時にタスク削除予定かどうかを判定。
	 * @param CurrentFrameDeltaTime 本フレームの DeltaTime
	 */
	bool IsScheduleDeleteTask(float CurrentFrameDeltaTime) const;

private:
	/** MID を生成 */
	bool CreateMaterialInstanceDynamic(UMaterialInstance* OwnerMaterial);
	/** 終了処理 (MID の GC など) */
	void Cleanup();
	/** WeightedBlendables を初期化 */
	void InitializeOverrideSettings();
private:
	// --------------------------------------------------------------------
	//  外部所有参照 – ライフタイム保証は UPostProcessCallSubsystem が担う
	// --------------------------------------------------------------------
	//note: このふたつのポインタはUPostProcessCallSubsystemよりもこのクラスのライフサイクルが短いことと、DatatableをUPROPERTYで保持しているため生ポインタで保持している
	const FTransientPostProcessConfig* PostProcessConfig{nullptr};
	UPostProcessCallSubsystem* Owner{};

	//note: このオブジェクトをGCオブジェクトとして保護
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
	 * @brief データテーブル ID を指定してエフェクトを再生。
	 * @param EffectID 行ID
	 * @return true: 再生開始, false: 失敗
	 */
	UFUNCTION(BlueprintCallable,Category="PostProcess", meta=(ToolTip="データテーブル上のIDに基づいてポストエフェクトを呼び出します"))
	bool PlayTransientPostProcess(const FName& EffectID);
	/**
	 * @brief 再生時にラムダでマテリアル初期化を行うバージョン。
	 * @param EffectID     行ID
	 * @param InitFunction MID への初期設定コールバック
	 */
	bool PlayTransientPostProcess(const FName& EffectID, const TFunctionRef<void(UMaterialInstanceDynamic*)>& InitFunction);
	/**
	 * @brief 指定 ID のエフェクトが再生中かチェック。
	 * @param EffectID 行ID
	 * @param InWorld  DeltaSeconds 取得に用いる World (NULL 可)
	 * @return 再生中なら true
	 */
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
