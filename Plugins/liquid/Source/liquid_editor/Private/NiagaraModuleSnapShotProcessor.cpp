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
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"

#define LOCTEXT_NAMESPACE "FNiagaraModuleSnapShotProcessor"

#pragma optimize( "", off )

namespace
{
	UEdGraphPin* FindParameterMapInputPin(const UEdGraphNode& Node)
	{
		for (UEdGraphPin* Pin : Node.Pins)
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

	void TraverseParameterMapChain(UEdGraphPin* StartPin, UNiagaraScript* TargetModule, TSet<UEdGraphNode*>& VisitedNodes,
		TArray<UNiagaraNodeFunctionCall*>& OutModuleNodes)
	{
		if (!StartPin)
		{
			return;
		}

		for (UEdGraphPin* LinkedPin : StartPin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}

			UEdGraphNode* Node = LinkedPin->GetOwningNode();
			if (!Node || VisitedNodes.Contains(Node))
			{
				continue;
			}

			VisitedNodes.Add(Node);

			if (UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node))
			{
				if (FunctionNode->FunctionScript == TargetModule)
				{
					OutModuleNodes.Add(FunctionNode);
				}
			}

			UEdGraphPin* InputPin = FindParameterMapInputPin(*Node);
			if (InputPin && InputPin != StartPin)
			{
				TraverseParameterMapChain(InputPin, TargetModule, VisitedNodes, OutModuleNodes);
			}
		}
	}

	void GatherModuleNodesForUsage(UNiagaraGraph* Graph, UNiagaraScript* TargetModule, ENiagaraScriptUsage Usage,
		TArray<UNiagaraNodeFunctionCall*>& OutModuleNodes)
	{
		if (!Graph || !TargetModule)
		{
			return;
		}

		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass(OutputNodes);

		for (UNiagaraNodeOutput* OutputNode : OutputNodes)
		{
			if (!OutputNode || OutputNode->GetUsage() != Usage)
			{
				continue;
			}

			UEdGraphPin* OutputInputPin = FindParameterMapInputPin(*OutputNode);
			if (!OutputInputPin)
			{
				continue;
			}

			TSet<UEdGraphNode*> VisitedNodes;
			TraverseParameterMapChain(OutputInputPin, TargetModule, VisitedNodes, OutModuleNodes);
		}
	}

	bool TrySetRapidIterationValue(const UNiagaraScript* SourceScript, const FString& FunctionName, const FString& InputName,
		const TArray<FNiagaraVariable>& RapidIterationVariables, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!SourceScript || !InputsObject.IsValid())
		{
			return false;
		}

		const FString Suffix = TEXT(".") + InputName;
		const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");
		const FNiagaraVariable* MatchedVariable = nullptr;
		int32 BestScore = -1;

		for (const FNiagaraVariable& Variable : RapidIterationVariables)
		{
			const FString VariableName = Variable.GetName().ToString();
			if (!VariableName.EndsWith(Suffix))
			{
				continue;
			}

			const int32 FunctionIndex = VariableName.Find(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			if (FunctionIndex == INDEX_NONE)
			{
				continue;
			}

			const int32 Score = VariableName.Len();
			if (Score > BestScore)
			{
				BestScore = Score;
				MatchedVariable = &Variable;
			}
		}

		if (!MatchedVariable)
		{
			return false;
		}

		const FNiagaraParameterStore& RapidIterationParameters = SourceScript->RapidIterationParameters;
		if (RapidIterationParameters.IndexOf(*MatchedVariable) == INDEX_NONE)
		{
			return false;
		}

		const FNiagaraTypeDefinition& InputType = MatchedVariable->GetType();
		if (InputType == FNiagaraTypeDefinition::GetFloatDef())
		{
			const float Value = RapidIterationParameters.GetParameterValue<float>(*MatchedVariable);
			InputsObject->SetNumberField(InputName, Value);
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetIntDef())
		{
			const int32 Value = RapidIterationParameters.GetParameterValue<int32>(*MatchedVariable);
			InputsObject->SetNumberField(InputName, Value);
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetBoolDef())
		{
			const FNiagaraBool Value = RapidIterationParameters.GetParameterValue<FNiagaraBool>(*MatchedVariable);
			InputsObject->SetBoolField(InputName, Value.GetValue());
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetVec2Def())
		{
			const FVector2f Value = RapidIterationParameters.GetParameterValue<FVector2f>(*MatchedVariable);
			TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
			VecObject->SetNumberField(TEXT("X"), Value.X);
			VecObject->SetNumberField(TEXT("Y"), Value.Y);
			InputsObject->SetObjectField(InputName, VecObject);
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetVec3Def())
		{
			const FVector3f Value = RapidIterationParameters.GetParameterValue<FVector3f>(*MatchedVariable);
			TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
			VecObject->SetNumberField(TEXT("X"), Value.X);
			VecObject->SetNumberField(TEXT("Y"), Value.Y);
			VecObject->SetNumberField(TEXT("Z"), Value.Z);
			InputsObject->SetObjectField(InputName, VecObject);
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetVec4Def())
		{
			const FVector4f Value = RapidIterationParameters.GetParameterValue<FVector4f>(*MatchedVariable);
			TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
			VecObject->SetNumberField(TEXT("X"), Value.X);
			VecObject->SetNumberField(TEXT("Y"), Value.Y);
			VecObject->SetNumberField(TEXT("Z"), Value.Z);
			VecObject->SetNumberField(TEXT("W"), Value.W);
			InputsObject->SetObjectField(InputName, VecObject);
			return true;
		}

		return false;
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
		GatherModuleNodesForUsage(ScriptSource->NodeGraph, TargetModule, Usage, FunctionNodes);

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
			TSet<FString> InputNames;
			GatherModuleInputNames(TargetModule, InputNames);

			TArray<FNiagaraVariable> RapidIterationVariables;
			SourceScript->RapidIterationParameters.GetParameters(RapidIterationVariables);

			const FString FunctionName = FunctionNode->GetFunctionName();
			for (const FString& InputName : InputNames)
			{
				TrySetRapidIterationValue(SourceScript, FunctionName, InputName, RapidIterationVariables, InputsObject);
			}

			if (InputsObject->Values.Num() == 0)
			{
				continue;
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
	//IFileManager::Get().MakeDirecfatory(*FPaths::GetPath(OutOutputPath), true);

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
