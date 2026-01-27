#include "NiagaraModuleSnapShotProcessor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FNiagaraModuleSnapShotProcessor"

#pragma optimize( "", off )

namespace
{
	bool TrySetLiteralInputValue(const UEdGraphPin* Pin, const FString& FieldName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Pin || !InputsObject.IsValid())
		{
			return false;
		}

		if (Pin->PinType.PinCategory == TEXT("ParameterMap"))
		{
			return false;
		}

		if (Pin->LinkedTo.Num() > 0)
		{
			return false;
		}

		FString ValueString;
		if (Pin->DefaultObject)
		{
			ValueString = Pin->DefaultObject->GetPathName();
		}
		else if (!Pin->DefaultValue.IsEmpty())
		{
			ValueString = Pin->DefaultValue;
		}
		else if (!Pin->DefaultTextValue.IsEmpty())
		{
			ValueString = Pin->DefaultTextValue.ToString();
		}

		if (ValueString.IsEmpty())
		{
			return false;
		}

		if (ValueString.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			InputsObject->SetBoolField(FieldName, true);
			return true;
		}
		if (ValueString.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			InputsObject->SetBoolField(FieldName, false);
			return true;
		}

		double NumericValue = 0.0;
		if (LexTryParseString(NumericValue, *ValueString))
		{
			InputsObject->SetNumberField(FieldName, NumericValue);
			return true;
		}

		InputsObject->SetStringField(FieldName, ValueString);
		return true;
	}

	UEdGraphPin* FindParameterMapInputPin(const UNiagaraNodeFunctionCall& FunctionCallNode)
	{
		for (UEdGraphPin* Pin : FunctionCallNode.Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				return Pin;
			}
		}

		return nullptr;
	}

	bool IsOverridePinForFunction(UEdGraphPin& OverridePin, const UNiagaraNodeFunctionCall& FunctionCallNode)
	{
		if (OverridePin.PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
		{
			return false;
		}

		FNiagaraParameterHandle InputHandle(OverridePin.PinName);
		return InputHandle.GetNamespace().ToString() == FunctionCallNode.GetFunctionName();
	}

	bool TryFindDefaultPinInModuleGraph(UNiagaraScript* ModuleScript, const FString& InputName, UEdGraphPin*& OutDefaultPin)
	{
		OutDefaultPin = nullptr;
		if (!ModuleScript)
		{
			return false;
		}

		UNiagaraScriptSource* ModuleSource = Cast<UNiagaraScriptSource>(ModuleScript->GetLatestSource());
		if (!ModuleSource || !ModuleSource->NodeGraph)
		{
			return false;
		}

		const auto ExtractInputName = [](const FString& FullName)
		{
			int32 DotIndex = INDEX_NONE;
			if (FullName.FindLastChar(TEXT('.'), DotIndex))
			{
				return FullName.Mid(DotIndex + 1);
			}
			return FullName;
		};

		for (UEdGraphNode* Node : ModuleSource->NodeGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapGet"))
			{
				continue;
			}

			FProperty* Property = Node->GetClass()->FindPropertyByName(TEXT("PinOutputToPinDefaultPersistentId"));
			FMapProperty* MapProperty = CastField<FMapProperty>(Property);
			if (!MapProperty)
			{
				continue;
			}

			void* MapPtr = MapProperty->ContainerPtrToValuePtr<void>(Node);
			FScriptMapHelper MapHelper(MapProperty, MapPtr);

			for (UEdGraphPin* OutputPin : Node->Pins)
			{
				if (!OutputPin || OutputPin->Direction != EGPD_Output || OutputPin->bOrphanedPin)
				{
					continue;
				}

				if (OutputPin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				const FString OutputName = OutputPin->PinName.ToString();
				const FString OutputInputName = ExtractInputName(OutputName);
				if (!OutputInputName.Equals(InputName))
				{
					continue;
				}

				const FGuid OutputGuid = OutputPin->PersistentGuid;
				FGuid DefaultGuid;
				bool bFoundDefaultGuid = false;
				for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
				{
					if (!MapHelper.IsValidIndex(Index))
					{
						continue;
					}

					const FGuid* KeyGuid = reinterpret_cast<FGuid*>(MapHelper.GetKeyPtr(Index));
					const FGuid* ValueGuid = reinterpret_cast<FGuid*>(MapHelper.GetValuePtr(Index));
					if (KeyGuid && ValueGuid && *KeyGuid == OutputGuid)
					{
						DefaultGuid = *ValueGuid;
						bFoundDefaultGuid = true;
						break;
					}
				}

				if (!bFoundDefaultGuid)
				{
					continue;
				}

				for (UEdGraphPin* InputPin : Node->Pins)
				{
					if (!InputPin || InputPin->Direction != EGPD_Input)
					{
						continue;
					}

					if (InputPin->PersistentGuid == DefaultGuid)
					{
						OutDefaultPin = InputPin;
						return true;
					}
				}
			}
		}

		return false;
	}

	void GatherModuleInputNames(UNiagaraScript* ModuleScript, TSet<FString>& OutInputNames)
	{
		if (!ModuleScript)
		{
			return;
		}

		UNiagaraScriptSource* ModuleSource = Cast<UNiagaraScriptSource>(ModuleScript->GetLatestSource());
		if (!ModuleSource || !ModuleSource->NodeGraph)
		{
			return;
		}

		for (UEdGraphNode* Node : ModuleSource->NodeGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapGet"))
			{
				continue;
			}

			for (UEdGraphPin* OutputPin : Node->Pins)
			{
				if (!OutputPin || OutputPin->Direction != EGPD_Output || OutputPin->bOrphanedPin)
				{
					continue;
				}

				if (OutputPin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				const FString OutputName = OutputPin->PinName.ToString();
				int32 DotIndex = INDEX_NONE;
				const FString InputName = OutputName.FindLastChar(TEXT('.'), DotIndex)
					? OutputName.Mid(DotIndex + 1)
					: OutputName;

				OutInputNames.Add(InputName);
			}
		}
	}

	FString ScriptUsageToString(ENiagaraScriptUsage Usage)
	{
		const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Usage)) : TEXT("Unknown");
	}

	bool AppendModuleSnapshots(UNiagaraScript* TargetModule, UNiagaraScript* SourceScript, ENiagaraScriptUsage Usage,
		const FString& UsageLabel, TArray<TSharedPtr<FJsonValue>>& OutModules)
	{
		if (!TargetModule || !SourceScript)
		{
			return false;
		}

		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceScript->GetLatestSource());
		if (!ScriptSource || !ScriptSource->NodeGraph)
		{
			return false;
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
			UEdGraphPin* ParameterMapInputPin = FindParameterMapInputPin(*FunctionNode);
			UEdGraphNode* OverrideNode = nullptr;
			if (ParameterMapInputPin && ParameterMapInputPin->LinkedTo.Num() == 1 && ParameterMapInputPin->LinkedTo[0])
			{
				OverrideNode = ParameterMapInputPin->LinkedTo[0]->GetOwningNode();
			}
			if (OverrideNode)
			{
				for (UEdGraphPin* OverridePin : OverrideNode->Pins)
				{
					if (!OverridePin || OverridePin->Direction != EGPD_Input)
					{
						continue;
					}

					if (!IsOverridePinForFunction(*OverridePin, *FunctionNode))
					{
						continue;
					}

					FNiagaraParameterHandle InputHandle(OverridePin->PinName);
					const FString FieldName = InputHandle.GetName().ToString();
					TrySetLiteralInputValue(OverridePin, FieldName, InputsObject);
				}
			}

			TSet<FString> InputNames;
			for (UEdGraphPin* Pin : FunctionNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input)
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				const FString PinName = Pin->PinName.ToString();
				if (PinName.Equals(TEXT("InputMap")) || PinName.Equals(TEXT("OutputMap")))
				{
					continue;
				}

				InputNames.Add(PinName);
			}

			GatherModuleInputNames(TargetModule, InputNames);

			for (const FString& InputName : InputNames)
			{
				if (InputsObject->HasField(InputName))
				{
					continue;
				}

				UEdGraphPin* DefaultPin = nullptr;
				if (TryFindDefaultPinInModuleGraph(TargetModule, InputName, DefaultPin))
				{
					TrySetLiteralInputValue(DefaultPin, InputName, InputsObject);
				}
			}

			ModuleObject->SetObjectField(TEXT("Inputs"), InputsObject);
			OutModules.Add(MakeShared<FJsonValueObject>(ModuleObject));
		}

		return OutModules.Num() > 0;
	}
}

bool FNiagaraModuleSnapshotProcessor::IsModuleScript(const UNiagaraScript* Script)
{
	return Script && Script->GetUsage() == ENiagaraScriptUsage::Module;
}

FString FNiagaraModuleSnapshotProcessor::NormalizeGamePath(const FString& Path)
{
	FString CleanPath = Path;
	CleanPath.TrimStartAndEndInline();
	while (CleanPath.EndsWith(TEXT("/")))
	{
		CleanPath.LeftChopInline(1);
	}
	return CleanPath;
}

bool FNiagaraModuleSnapshotProcessor::IsValidGamePath(const FString& Path)
{
	const FString CleanPath = NormalizeGamePath(Path);
	return CleanPath.StartsWith(TEXT("/Game")) && FPackageName::IsValidLongPackageName(CleanPath, false);
}

bool FNiagaraModuleSnapshotProcessor::CreateSnapshot(UNiagaraScript* ModuleScript, const FString& SearchPath, FString& OutErrorMessage, FString& OutOutputPath)
{
	OutErrorMessage.Reset();
	OutOutputPath.Reset();

	if (!IsModuleScript(ModuleScript))
	{
		OutErrorMessage = TEXT("Please select a Niagara Module Script asset.");
		return false;
	}

	if (!IsValidGamePath(SearchPath))
	{
		OutErrorMessage = TEXT("Please enter a valid /Game path.");
		return false;
	}

	const FString NormalizedPath = NormalizeGamePath(SearchPath);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassNames.Add(UNiagaraSystem::StaticClass()->GetFName());
	Filter.PackagePaths.Add(*NormalizedPath);
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

					TArray<TSharedPtr<FJsonValue>> UsageModules;
					if (AppendModuleSnapshots(ModuleScript, Script, Usage, ScriptUsageToString(Usage), UsageModules))
					{
						ModuleArray.Append(UsageModules);
					}
				}
			}

			if (ModuleArray.Num() > 0)
			{
				TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
				EmitterObject->SetStringField(TEXT("Emitter"), Handle.GetName().ToString());
				EmitterObject->SetArrayField(TEXT("Modules"), ModuleArray);
				EmitterArray.Add(MakeShared<FJsonValueObject>(EmitterObject));
			}
		}

		if (EmitterArray.Num() > 0)
		{
			SystemObject->SetArrayField(TEXT("Emitters"), EmitterArray);
			SystemArray.Add(MakeShared<FJsonValueObject>(SystemObject));
		}
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("ModuleScript"), ModuleScript->GetPathName());
	RootObject->SetArrayField(TEXT("Systems"), SystemArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	OutputString.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	OutputString.ReplaceInline(TEXT("\r"), TEXT("\n"));
	OutputString.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
	OutputString.TrimEndInline();

	OutOutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("NiagaraModuleSnapshotsResult.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutOutputPath), true);

	const bool bSaved = FFileHelper::SaveStringToFile(OutputString, *OutOutputPath, FFileHelper::EEncodingOptions::ForceUTF8);
	if (!bSaved)
	{
		OutErrorMessage = TEXT("Failed to save NiagaraModuleSnapshotsResult.json.");
		return false;
	}

	return true;
}

#pragma optimize( "", on )

#undef LOCTEXT_NAMESPACE
