// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

/**
 * TRuntimeAssetPtr
 *
 *  - SoftObjectPtrの非同期ロードヘルパークラス
 *  - LoadAsync() を呼ぶと非同期ロード
 *  - ロード完了後は Get() で即利用
 *  - ロードが終わるまで Get() は nullptr を返す
 *  - このオブジェクト自身はロードしたAssetのGC保護をしないので注意
 */
template<typename AssetType>
class LIQUID_API TRuntimeAssetPtr
{
	static_assert(TIsDerivedFrom<AssetType, UObject>::IsDerived, "AssetType must derive from UObject");
public:
	using ThisType = TRuntimeAssetPtr<AssetType>;

	TRuntimeAssetPtr() =default;
	explicit TRuntimeAssetPtr(const TSoftObjectPtr<AssetType>& SoftPtr)
		: SoftPtr(SoftPtr){}
	~TRuntimeAssetPtr() { Reset(); }

	void SetSoftPtr(const TSoftObjectPtr<AssetType>& InSoftPtr)
	{
		Reset();
		SoftPtr = InSoftPtr;
	}

	AssetType* Get() const
	{
		return CachedAssetPtr.Get();
	}

	void Reset()
	{
		//CancelHandle()でCallbackが呼ばれることがあるようなので先に無効化しておく(GPT:o3が5.3以降で報告ありと言ってきたので一応)
		LoadedCallback = nullptr;
		if (LoadingHandle.IsValid())
		{
			LoadingHandle->CancelHandle();
		}
		LoadingHandle.Reset();
		CachedAssetPtr.Reset();
		SoftPtr.Reset();
	}
	bool IsLoaded()const{return CachedAssetPtr.IsValid();}
	bool IsLoading()const{return LoadingHandle.IsValid() && !LoadingHandle->HasLoadCompleted();}

	void LoadAsync(TFunction<void(AssetType*)> Callback = nullptr,
				   int32 Priority = FStreamableManager::AsyncLoadHighPriority)
	{
		if (SoftPtr.IsNull())
		{
			return;
		}

		if (IsLoaded())
		{
			if (Callback)
			{
				Callback(CachedAssetPtr.Get());
			}
			return;
		}
		if (IsLoading())
		{
			UE_LOG(
			LogTemp,
			Warning,
			TEXT("[TRuntimeAssetPtr] LoadAsync called again during LoadAsync: %s"),
			*SoftPtr.ToString());
			return;
		}

		LoadedCallback = Callback;
		
		FStreamableManager& Manager = UAssetManager::GetStreamableManager();
		LoadingHandle = Manager.RequestAsyncLoad(
			SoftPtr.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				this->OnAsyncLoadCompleted();
			}),
			Priority);
		//アセットがすでにロード済だった場合でHandleがCompleteになっている時の対策
		if (LoadingHandle && LoadingHandle->HasLoadCompleted())
		{
			LoadingHandle.Reset();         
		}

	}

private:
	void OnAsyncLoadCompleted()
	{
		auto Result = SoftPtr.Get();
		bool IsSuccessful = SoftPtr.IsValid() && Result != nullptr;
		if (!IsSuccessful)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("[TRuntimeAssetPtr] Failed to load asset: %s"),
				   *SoftPtr.ToString());
		}

		CachedAssetPtr = IsSuccessful ? Result : nullptr;
		//ロードが終了したので破棄
		LoadingHandle.Reset();
		if (LoadedCallback)
		{
			LoadedCallback(CachedAssetPtr.Get());
		}
		LoadedCallback = nullptr;
	}

private:
	TSoftObjectPtr<AssetType> SoftPtr{};
	TWeakObjectPtr<AssetType> CachedAssetPtr{}; // Note: GCから守る強参照は不要：呼び出し側(このオブジェクトの保持者が持つ)
	TSharedPtr<FStreamableHandle> LoadingHandle{};
	TFunction<void(AssetType*)> LoadedCallback{nullptr};
};
