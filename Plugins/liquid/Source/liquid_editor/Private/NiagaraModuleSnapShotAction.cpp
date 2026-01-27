#include "NiagaraModuleSnapShotAction.h"

#include "AssetRegistry/AssetData.h"
#include "AssetSelection.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "NiagaraModuleSnapShotProcessor.h"
#include "NiagaraScript.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FNiagaraModuleSnapShotAction"

namespace
{
	struct FSnapshotDialogState : public TSharedFromThis<FSnapshotDialogState>
	{
		UNiagaraScript* SelectedScript = nullptr;
		bool bIsModuleScript = false;
		FString SearchPath = TEXT("/Game/test_assetactions");
		bool bAccepted = false;
		TWeakPtr<SWindow> Window;

		bool IsValid() const
		{
			return bIsModuleScript && FNiagaraModuleSnapshotProcessor::IsValidGamePath(SearchPath);
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
					State->bIsModuleScript = FNiagaraModuleSnapshotProcessor::IsModuleScript(LoadedScript);
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
		OutPath = FNiagaraModuleSnapshotProcessor::NormalizeGamePath(State->SearchPath);
		return State->IsValid();
	}
}

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

	if (!FNiagaraModuleSnapshotProcessor::IsModuleScript(ModuleScript))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InvalidModuleScript", "Please select a Niagara Module Script asset."));
		return;
	}

	FString ErrorMessage;
	FString OutputPath;
	if (!FNiagaraModuleSnapshotProcessor::CreateSnapshot(ModuleScript, SearchPath, ErrorMessage, OutputPath))
	{
		const FText ErrorText = ErrorMessage.IsEmpty()
			? LOCTEXT("SnapshotSaveFailed", "Failed to save NiagaraModuleSnapshotsResult.json.")
			: FText::FromString(ErrorMessage);
		FMessageDialog::Open(EAppMsgType::Ok, ErrorText);
		return;
	}

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SnapshotSaveSucceeded", "Niagara module snapshot saved to Saved/NiagaraModuleSnapshotsResult.json."));
}

#undef LOCTEXT_NAMESPACE
