#include "NiagaraModuleSnapShot.h"

#include "AssetSelection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "PropertyCustomizationHelpers.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FNiagaraModuleSnapShot"

#pragma optimize( "", off )

namespace
{
	bool IsModuleScript(const UNiagaraScript* Script)
	{
		return Script && Script->GetUsage() == ENiagaraScriptUsage::Module;
	}

	FString NormalizeGamePath(const FString& Path)
	{
		FString CleanPath = Path;
		CleanPath.TrimStartAndEndInline();
		while (CleanPath.EndsWith(TEXT("/")))
		{
			CleanPath.LeftChopInline(1);
		}
		return CleanPath;
	}

	bool IsValidGamePath(const FString& Path)
	{
		const FString CleanPath = NormalizeGamePath(Path);
		return CleanPath.StartsWith(TEXT("/Game")) && FPackageName::IsValidLongPackageName(CleanPath, false);
	}

	FString PinValueToString(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}

		if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0])
		{
			return FString::Printf(TEXT("Linked:%s"), *Pin->LinkedTo[0]->PinName.ToString());
		}

		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}

		if (!Pin->DefaultValue.IsEmpty())
		{
			return Pin->DefaultValue;
		}

		if (!Pin->DefaultTextValue.IsEmpty())
		{
			return Pin->DefaultTextValue.ToString();
		}

		return FString();
	}

	FString ScriptUsageToString(ENiagaraScriptUsage Usage)
	{
		const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Usage)) : TEXT("Unknown");
	}

	struct FSnapshotDialogState : public TSharedFromThis<FSnapshotDialogState>
	{
		UNiagaraScript* SelectedScript = nullptr;
		bool bIsModuleScript = false;
		FString SearchPath = TEXT("/Game");
		bool bAccepted = false;
		TWeakPtr<SWindow> Window;

		bool IsValid() const
		{
			return bIsModuleScript && IsValidGamePath(SearchPath);
		}
	};

	bool ShowSnapshotDialog(UNiagaraScript*& OutScript, FString& OutPath)
	{
		TSharedRef<FSnapshotDialogState> State = MakeShared<FSnapshotDialogState>();

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("SnapshotDialogTitle", "Niagara Module Snapshot"))
			.ClientSize(FVector2D(520.0f, 180.0f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		State->Window = Window;

		Window->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ModuleLabel", "Niagara Module Script"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UNiagaraScript::StaticClass())
				.ObjectPath_Lambda([State]()
				{
					return State->SelectedScript ? State->SelectedScript->GetPathName() : FString();
				})
				.OnObjectChanged_Lambda([State](const FAssetData& AssetData)
				{
					UNiagaraScript* LoadedScript = Cast<UNiagaraScript>(AssetData.GetAsset());
					if (!LoadedScript && AssetData.IsValid())
					{
						LoadedScript = Cast<UNiagaraScript>(AssetData.ToSoftObjectPath().TryLoad());
					}

					State->SelectedScript = LoadedScript;
					State->bIsModuleScript = IsModuleScript(LoadedScript);
				})
				.OnShouldFilterAsset_Lambda([](const FAssetData& AssetData)
				{
					return AssetData.AssetClassPath != UNiagaraScript::StaticClass()->GetClassPathName();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PathLabel", "Search Path (/Game...)"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([State]() { return FText::FromString(State->SearchPath); })
				.OnTextChanged_Lambda([State](const FText& Text)
				{
					State->SearchPath = Text.ToString();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(4.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("SnapshotOk", "OK"))
					.IsEnabled_Lambda([State]() { return State->IsValid(); })
					.OnClicked_Lambda([State]()
					{
						State->bAccepted = true;
						if (State->Window.IsValid())
						{
							State->Window.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("SnapshotCancel", "Cancel"))
					.OnClicked_Lambda([State]()
					{
						State->bAccepted = false;
						if (State->Window.IsValid())
						{
							State->Window.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]
		);

		FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr));

		if (!State->bAccepted)
		{
			return false;
		}

		OutScript = State->SelectedScript;
		OutPath = NormalizeGamePath(State->SearchPath);
		return State->IsValid();
	}

	void AppendModuleSnapshots(UNiagaraScript* TargetModule, UNiagaraScript* SourceScript, const FString& UsageLabel,
		TArray<TSharedPtr<FJsonValue>>& OutModules)
	{
		if (!TargetModule || !SourceScript)
		{
			return;
		}

		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceScript->GetLatestSource());
		if (!ScriptSource || !ScriptSource->NodeGraph)
		{
			return;
		}

		TArray<UNiagaraNodeFunctionCall*> FunctionNodes;
		ScriptSource->NodeGraph->GetNodesOfClass(FunctionNodes);

		for (UNiagaraNodeFunctionCall* FunctionNode : FunctionNodes)
		{
			if (!FunctionNode)
			{
				continue;
			}

			UNiagaraScript* FunctionScript = FunctionNode->FunctionScript;
			if (FunctionScript != TargetModule)
			{
				continue;
			}

			TSharedPtr<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
			ModuleObject->SetStringField(TEXT("ModuleScript"), TargetModule->GetPathName());
			ModuleObject->SetStringField(TEXT("ScriptUsage"), UsageLabel);

			TSharedPtr<FJsonObject> InputsObject = MakeShared<FJsonObject>();
			for (UEdGraphPin* Pin : FunctionNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || Pin->bHidden)
				{
					continue;
				}

				InputsObject->SetStringField(Pin->PinName.ToString(), PinValueToString(Pin));
			}

			ModuleObject->SetObjectField(TEXT("Inputs"), InputsObject);
			OutModules.Add(MakeShared<FJsonValueObject>(ModuleObject));
		}
	}
}


/*bool UNiagaraModuleSnapshotAction::CanExecuteOnAssets(const TArray<FAssetData>& InAssets) const
{
	return InAssets.Num() > 0;
}*/

void UNiagaraModuleSnapshotAction::CreateNiagaraModuleSnapshot()
{
	TArray<FAssetData> SelectedObjects;
	AssetSelectionUtils::GetSelectedAssets(SelectedObjects);
	if (SelectedObjects.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSelection", "Please select any asset before running the snapshot action."));
		return;
	}

	UNiagaraScript* ModuleScript = nullptr;
	FString SearchPath;

	if (!ShowSnapshotDialog(ModuleScript, SearchPath))
	{
		return;
	}

	if (!IsModuleScript(ModuleScript))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InvalidModuleScript", "Please select a Niagara Module Script asset."));
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassNames.Add(UNiagaraSystem::StaticClass()->GetFName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> SystemAssets;
	AssetRegistryModule.Get().GetAssets(Filter, SystemAssets);

	TArray<TSharedPtr<FJsonValue>> SystemArray;
	for (const FAssetData& AssetData : SystemAssets)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(AssetData.GetAsset());
		if (!System)
		{
			continue;
		}

		TSharedPtr<FJsonObject> SystemObject = MakeShared<FJsonObject>();
		SystemObject->SetStringField(TEXT("System"), AssetData.ObjectPath.ToString());

		TArray<TSharedPtr<FJsonValue>> EmitterArray;
		const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
		for (const FNiagaraEmitterHandle& Handle : Handles)
		{
			TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
			EmitterObject->SetStringField(TEXT("Emitter"), Handle.GetName().ToString());

			TArray<TSharedPtr<FJsonValue>> ModuleArray;
			const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
			if (EmitterData)
			{
				const TArray<ENiagaraScriptUsage> Usages = {
					ENiagaraScriptUsage::EmitterSpawnScript,
					ENiagaraScriptUsage::EmitterUpdateScript,
					ENiagaraScriptUsage::ParticleSpawnScript,
					ENiagaraScriptUsage::ParticleUpdateScript
				};

				for (ENiagaraScriptUsage Usage : Usages)
				{
					UNiagaraScript* Script = EmitterData->GetScript(Usage, FGuid());
					if (!Script)
					{
						continue;
					}

					AppendModuleSnapshots(ModuleScript, Script, ScriptUsageToString(Usage), ModuleArray);
				}
			}

			EmitterObject->SetArrayField(TEXT("Modules"), ModuleArray);
			EmitterArray.Add(MakeShared<FJsonValueObject>(EmitterObject));
		}

		SystemObject->SetArrayField(TEXT("Emitters"), EmitterArray);
		SystemArray.Add(MakeShared<FJsonValueObject>(SystemObject));
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("ModuleScript"), ModuleScript->GetPathName());
	RootObject->SetArrayField(TEXT("Systems"), SystemArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	OutputString.ReplaceInline(TEXT("\n"), TEXT("\r\n"));

	const FString OutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("NiagaraModuleSnapshotsResult.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);

	const bool bSaved = FFileHelper::SaveStringToFile(OutputString, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8);
	if (!bSaved)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SnapshotSaveFailed", "Failed to save NiagaraModuleSnapshotsResult.json."));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SnapshotSaveSucceeded", "Niagara module snapshot saved to Saved/NiagaraModuleSnapshotsResult.json."));
	}
}

#pragma optimize( "", on)

#undef LOCTEXT_NAMESPACE
