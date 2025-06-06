// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "PostProcessCallSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FPostprocessControlParameters
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,meta=(ToolTip="操作するマテリアルパラメータ名"))
	FName MaterialParameterName = NAME_None;
	UPROPERTY(EditAnywhere,meta=(ToolTip="パラメータを操作するFloatカーブアセット"))
	TObjectPtr<UCurveFloat> FloatCurve{};
};

USTRUCT(BlueprintType)
struct FOverridePostProcessConfig : public FTableRowBase
{
	GENERATED_BODY()	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> Material{nullptr}; //TSoftObjectPtrの方がいいかも

	UPROPERTY(EditAnywhere,BlueprintReadOnly, meta=(ToolTip="このポストプロセスのアルファ値(0で何もしない、1で完全適用)"))
	float Weight = 1.0f;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, meta=(ToolTip="適用する時間"))
	float Duration = 2.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,meta=(ToolTip="操作するマテリアルパラメータ"))
	TArray<FPostprocessControlParameters> ControlParameters;
};

enum class PostProcessTaskTickResult : uint8
{
	Progress,
	Finish,
	Failed,
};

class FPostProcessOverrideTask : FGCObject
{
public:
	explicit FPostProcessOverrideTask(const FOverridePostProcessConfig* ConfigPtr, UPostProcessCallSubsystem* Owner);

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	bool Activate();
	PostProcessTaskTickResult Tick(APlayerCameraManager* CameraManager, float DeltaTime);

private:
	//memo: このふたつのポインタはUPostProcessCallSubsystemよりもこのクラスのライフサイクルが短いことと、DatatableをUPROPERTYで保持しているため生ポインタで保持している
	const FOverridePostProcessConfig* PostProcessConfig{nullptr};
	UPostProcessCallSubsystem* Owner{};

	//memo: このオブジェクトをGCオブジェクトとして保護
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstanceDynamic{nullptr};
	FPostProcessSettings OverrideSettings{};
	float ElapsedTime = .0f;
};

/**
 * 
 */
UCLASS(Config=Game)
class LIQUID_API UPostProcessCallSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	
	UFUNCTION(BlueprintCallable,Category="PostProcess")
	void PlayPostEffect(const FName EffectID);

private:
	void BeginEffect(const FOverridePostProcessConfig* Config);
	void OnWorldPostActorTick(UWorld* InWorld, ELevelTick InType,float DeltaTime);
	
	UPROPERTY()
	TObjectPtr<UDataTable> PostProcessTable{nullptr};
	
	TArray<TUniquePtr<FPostProcessOverrideTask>> OverrideTasks;
	FDelegateHandle PostActorTickHandle;
	
	static constexpr TCHAR TableAssetPath[] = TEXT("/liquid/post_process/sample_table");
};
