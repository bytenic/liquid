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
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptExecutionParameterStore.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Curves/RichCurve.h"
#include "UObject/UnrealType.h"
#include "NiagaraParameterStore.h"

#define LOCTEXT_NAMESPACE "FNiagaraModuleSnapShotProcessor"

#pragma optimize( "", off )

namespace
{
	TSharedPtr<FJsonObject> BuildCurveKeyObject(const FRichCurveKey& Key);
	TArray<TSharedPtr<FJsonValue>> BuildCurveKeysArray(const FRichCurve& Curve);
	void AddDebugField(const TSharedPtr<FJsonObject>& InputsObject, const FString& Key, const FString& Value);
	void AddDebugArrayField(const TSharedPtr<FJsonObject>& InputsObject, const FString& Key, const TArray<FString>& Values);
	UNiagaraDataInterface* FindCurveDataInterfaceFromNode(const UEdGraphNode* Node);
	UEdGraphPin* FindParameterMapSetInputPin(UEdGraphNode* SetNode, const FString& InputName, const FString& FunctionName);
	void GatherDataInterfacesFromObject(const UObject* Owner, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void GatherStructDebug(const void* StructPtr, const UScriptStruct* StructType, TArray<FString>& OutProps, TArray<FString>& OutObjects, TArray<FString>& OutObjectArrays);
	void GatherRapidIterationDataInterfacesFromParameters(const FNiagaraParameters& Parameters, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void FindDataInterfacesInStruct(const void* StructPtr, const UStruct* StructType, const FString& Path, TArray<FString>& OutEntries, int32 Depth);
	void CollectDataInterfacesFromStruct(const void* StructPtr, const UStruct* StructType, TArray<UNiagaraDataInterface*>& OutDataInterfaces, int32 Depth);
	void CollectCurveDataInterfacesFromGraph(UNiagaraGraph* Graph, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void CollectCurveDataInterfacesFromFunctionCall(UNiagaraNodeFunctionCall* FunctionNode, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void CollectCurveDataInterfacesFromDynamicInputs(UNiagaraGraph* Graph, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void CollectDynamicInputNodeDebug(UNiagaraNodeFunctionCall* FunctionNode, TArray<FString>& OutEntries);
	void GatherInputNamesFromFunctionCallPins(UNiagaraNodeFunctionCall* FunctionNode, TSet<FString>& OutInputNames);
	void GatherInputNamesFromRapidIteration(const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, TSet<FString>& OutInputNames);
	TSharedPtr<FJsonObject> BuildCurveObjectFromDataInterface(UNiagaraDataInterface* DataInterface);
	UEdGraphPin* FindFunctionInputPin(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName);
	bool TrySetStaticSwitchValue(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	struct FCurveChannel;
	void CollectCurveChannelsFromStruct(const void* StructPtr, const UStruct* StructType, int32 Depth, TArray<FCurveChannel>& OutChannels);
	bool GetCurveChannelsFromDataInterface(UNiagaraDataInterface* DataInterface, TArray<FCurveChannel>& OutChannels);
	bool HasCurveDataInterface(UNiagaraDataInterface* DataInterface);
	UNiagaraDataInterface* SelectOuterCurveInterfaceForDynamicInput(const TArray<UNiagaraDataInterface*>& Interfaces,
		const FString& ScriptPath, const FString& DynamicInputName, const FString& InputName);
	bool TrySetCurvesFromFunctionCallNode(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject,
		const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, const FNiagaraParameterStore& RapidIterationParameters,
		const TArray<UNiagaraDataInterface*>& OuterCurveInterfaces, const FString& SourceScriptPath);
	void CollectFunctionCallPinDebug(UNiagaraNodeFunctionCall* FunctionNode, TArray<FString>& OutEntries);
	void CollectDataInterfacesFromObjectDeep(const UObject* Owner, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	void CollectCurveInterfacesFromOuter(const UObject* Outer, TArray<UNiagaraDataInterface*>& OutDataInterfaces);
	UNiagaraDataInterface* SelectOuterCurveInterface(const TArray<UNiagaraDataInterface*>& Interfaces, const FString& ScriptPath, const FString& InputName);
	bool TryOverrideCurveScale(const FString& FunctionName, const FString& InputName, const TArray<FNiagaraVariable>& RapidIterationVariables,
		const FNiagaraParameterStore& RapidIterationParameters, const TSharedPtr<FJsonObject>& CurveObject);
	bool IsCurveInputName(const FString& InputName);
	UEdGraphPin* FindParameterMapSetInputPinInGraph(UNiagaraGraph* Graph, const FString& InputName, const FString& FunctionName);
	bool TrySetDynamicInputCurveValue(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject,
		const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, const FNiagaraParameterStore& RapidIterationParameters,
		const TArray<UNiagaraDataInterface*>& OuterCurveInterfaces, const FString& SourceScriptPath);
	FString GetUniqueInputFieldName(const TSharedPtr<FJsonObject>& InputsObject, const FString& InputName, const FString& Suffix);
	bool TrySetBoolFromPin(UEdGraphPin* Pin, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromPin(UEdGraphPin* Pin, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromFunctionNode(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromModuleScript(UNiagaraScript* ModuleScript, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromModuleRapidIteration(UNiagaraScript* ModuleScript, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromGraphMetadata(UNiagaraGraph* Graph, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool TrySetDefaultValueFromParameterMapGetNode(UEdGraphNode* GetNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	void AddDebugModuleGraphValuePins(UNiagaraGraph* Graph, const TSharedPtr<FJsonObject>& InputsObject);
	UEdGraphPin* FindParameterMapGetValuePin(UEdGraphNode* GetNode, const FString& InputName);
	FString GetPinDefaultString(UEdGraphPin* Pin);
	bool TrySetDefaultValueFromCalledGraphInputs(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject);
	bool IsBoolPinType(const UEdGraphPin* Pin);
	bool TryReadBoolFromNiagaraVariable(const FNiagaraVariable& Variable, bool& OutValue);
	FString NormalizeInputToken(const FString& In);
	bool IsSameInputToken(const FString& A, const FString& B);
	bool TrySetDefaultFromMetadataStruct(const void* StructPtr, const UStruct* StructType, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject, int32 Depth);
	bool TryExtractVariableLikeName(const void* KeyPtr, const FProperty* KeyProp, FString& OutName);
	bool TrySetCurveScaleFieldFromDataInterface(const UNiagaraDataInterface* DataInterface, const TSharedPtr<FJsonObject>& CurveObject);

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

	float ExtractCurveScale(const UNiagaraDataInterface* DataInterface)
	{
		if (!DataInterface)
		{
			return 1.0f;
		}

		const FFloatProperty* ScaleProperty = FindFProperty<FFloatProperty>(DataInterface->GetClass(), TEXT("CurveScale"));
		return ScaleProperty ? ScaleProperty->GetPropertyValue_InContainer(DataInterface) : 1.0f;
	}

	bool TrySetCurveScaleFieldFromDataInterface(const UNiagaraDataInterface* DataInterface, const TSharedPtr<FJsonObject>& CurveObject)
	{
		if (!DataInterface || !CurveObject.IsValid())
		{
			return false;
		}

		if (const FFloatProperty* FloatScale = FindFProperty<FFloatProperty>(DataInterface->GetClass(), TEXT("CurveScale")))
		{
			CurveObject->SetNumberField(TEXT("CurveScale"), FloatScale->GetPropertyValue_InContainer(DataInterface));
			return true;
		}

		const FStructProperty* StructScale = FindFProperty<FStructProperty>(DataInterface->GetClass(), TEXT("CurveScale"));
		if (!StructScale || !StructScale->Struct)
		{
			return false;
		}

		const void* ScalePtr = StructScale->ContainerPtrToValuePtr<void>(DataInterface);
		if (!ScalePtr)
		{
			return false;
		}

		TSharedPtr<FJsonObject> ScaleObject = MakeShared<FJsonObject>();
		const UStruct* ScaleStruct = StructScale->Struct;
		for (const TCHAR* ComponentName : {TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")})
		{
			if (const FFloatProperty* ComponentProp = FindFProperty<FFloatProperty>(ScaleStruct, ComponentName))
			{
				ScaleObject->SetNumberField(ComponentName, ComponentProp->GetPropertyValue_InContainer(ScalePtr));
				continue;
			}

			if (const FDoubleProperty* ComponentDoubleProp = FindFProperty<FDoubleProperty>(ScaleStruct, ComponentName))
			{
				ScaleObject->SetNumberField(ComponentName, ComponentDoubleProp->GetPropertyValue_InContainer(ScalePtr));
				continue;
			}
		}

		if (ScaleObject->Values.Num() == 0)
		{
			return false;
		}

		CurveObject->SetObjectField(TEXT("CurveScale"), ScaleObject);
		return true;
	}

	void GatherRapidIterationDataInterfaces(const FNiagaraParameterStore& Store, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		const UScriptStruct* StoreStruct = FNiagaraParameterStore::StaticStruct();
		if (!StoreStruct)
		{
			return;
		}

		const FArrayProperty* DIsProp = FindFProperty<FArrayProperty>(StoreStruct, TEXT("DataInterfaces"));
		const FObjectPropertyBase* ObjProp = DIsProp ? CastField<FObjectPropertyBase>(DIsProp->Inner) : nullptr;
		if (DIsProp && ObjProp)
		{
			FScriptArrayHelper DIsHelper(DIsProp, DIsProp->ContainerPtrToValuePtr<void>(&Store));
			const int32 Num = DIsHelper.Num();
			OutDataInterfaces.Reserve(Num);

			for (int32 Index = 0; Index < Num; ++Index)
			{
				const void* ObjPtr = DIsHelper.GetRawPtr(Index);
				if (!ObjPtr)
				{
					OutDataInterfaces.Add(nullptr);
					continue;
				}

				OutDataInterfaces.Add(Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue(ObjPtr)));
			}
		}

		if (OutDataInterfaces.Num() == 0)
		{
			const FArrayProperty* UObjectsProp = FindFProperty<FArrayProperty>(StoreStruct, TEXT("UObjects"));
			const FObjectPropertyBase* UObjectProp = UObjectsProp ? CastField<FObjectPropertyBase>(UObjectsProp->Inner) : nullptr;
			if (UObjectsProp && UObjectProp)
			{
				FScriptArrayHelper UObjectsHelper(UObjectsProp, UObjectsProp->ContainerPtrToValuePtr<void>(&Store));
				const int32 Num = UObjectsHelper.Num();
				OutDataInterfaces.Reserve(Num);

				for (int32 Index = 0; Index < Num; ++Index)
				{
					const void* ObjPtr = UObjectsHelper.GetRawPtr(Index);
					if (!ObjPtr)
					{
						continue;
					}

					if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(UObjectProp->GetObjectPropertyValue(ObjPtr)))
					{
						OutDataInterfaces.Add(DataInterface);
					}
				}
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
		const TArray<FNiagaraVariable>& RapidIterationVariables, const TSharedPtr<FJsonObject>& InputsObject,
		const TArray<UNiagaraDataInterface*>& FallbackDataInterfaces, const TArray<UNiagaraDataInterface*>& GraphCurveInterfaces,
		const TArray<UNiagaraDataInterface*>& OuterCurveInterfaces)
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

		auto TrySetRapidIterationDataInterfaceByName = [&](const FNiagaraParameterStore& Store) -> bool
		{
			const UScriptStruct* StoreStruct = FNiagaraParameterStore::StaticStruct();
			if (!StoreStruct)
			{
				AddDebugField(InputsObject, TEXT("_DebugRapidStoreStruct"), TEXT("Missing"));
				return false;
			}

			const FMapProperty* OffsetsProp = FindFProperty<FMapProperty>(StoreStruct, TEXT("ParameterOffsets"));
			if (OffsetsProp && OffsetsProp->KeyProp)
			{
				const FStructProperty* KeyStructProp = CastField<FStructProperty>(OffsetsProp->KeyProp);
				if (KeyStructProp)
				{
					TArray<FString> OffsetKeys;
					FScriptMapHelper MapHelper(OffsetsProp, OffsetsProp->ContainerPtrToValuePtr<void>(&Store));
					const int32 Num = MapHelper.Num();
					const FString Suffix = TEXT(".") + InputName;
					const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");
					AddDebugField(InputsObject, TEXT("_DebugRapidOffsetsCount"), FString::FromInt(Num));

					for (int32 Index = 0; Index < Num; ++Index)
					{
						if (!MapHelper.IsValidIndex(Index))
						{
							continue;
						}

						const uint8* PairPtr = MapHelper.GetPairPtr(Index);
						const void* KeyPtr = OffsetsProp->KeyProp->ContainerPtrToValuePtr<void>(PairPtr);
						const FNiagaraVariableBase* VarBase = static_cast<const FNiagaraVariableBase*>(KeyPtr);
						if (!VarBase)
						{
							continue;
						}

						const FString VarName = VarBase->GetName().ToString();
						OffsetKeys.Add(VarName);
						if (VarName.IsEmpty() || !VarName.EndsWith(Suffix) ||
							!VarName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
						{
							continue;
						}

						FNiagaraVariable Var(VarBase->GetType(), VarBase->GetName());
						UNiagaraDataInterface* DataInterface = Store.GetDataInterface(Var);
						if (!DataInterface)
						{
							continue;
						}

						if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DataInterface))
						{
							TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, SourceScript->RapidIterationParameters, CurveObject);
							InputsObject->SetObjectField(InputName, CurveObject);
							return true;
						}
					}

					AddDebugArrayField(InputsObject, TEXT("_DebugRapidOffsetsKeys"), OffsetKeys);
				}
			}

			const FArrayProperty* VarsProp = FindFProperty<FArrayProperty>(StoreStruct, TEXT("DataInterfaceVariables"));
			const FArrayProperty* DIsProp = FindFProperty<FArrayProperty>(StoreStruct, TEXT("DataInterfaces"));
			if (!VarsProp || !DIsProp)
			{
				AddDebugField(InputsObject, TEXT("_DebugRapidDIProps"), TEXT("Missing"));
				return false;
			}

			const FStructProperty* VarStructProp = CastField<FStructProperty>(VarsProp->Inner);
			const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(DIsProp->Inner);
			if (!VarStructProp || !ObjProp)
			{
				return false;
			}

			FScriptArrayHelper VarsHelper(VarsProp, VarsProp->ContainerPtrToValuePtr<void>(&Store));
			FScriptArrayHelper DIsHelper(DIsProp, DIsProp->ContainerPtrToValuePtr<void>(&Store));
			const int32 Num = FMath::Min(VarsHelper.Num(), DIsHelper.Num());
			if (Num <= 0)
			{
				return false;
			}

			TArray<FString> DataInterfaceVarNames;
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const void* VarPtr = VarsHelper.GetRawPtr(Index);
				const void* ObjPtr = DIsHelper.GetRawPtr(Index);
				if (!VarPtr || !ObjPtr)
				{
					continue;
				}

				const FNiagaraVariable* Var = static_cast<const FNiagaraVariable*>(VarPtr);
				const FString VarName = Var ? Var->GetName().ToString() : FString();
				if (!VarName.IsEmpty())
				{
					DataInterfaceVarNames.Add(VarName);
				}
				const FString Suffix = TEXT(".") + InputName;
				const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");
				if (VarName.IsEmpty() || !VarName.EndsWith(Suffix) || !VarName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
				{
					continue;
				}

				UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue(ObjPtr));
				if (!DataInterface)
				{
					continue;
				}

				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DataInterface))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, SourceScript->RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			AddDebugArrayField(InputsObject, TEXT("_DebugRapidDIVariables"), DataInterfaceVarNames);
			return false;
		};

		auto TrySetExecutableDataInterfaceByName = [&]() -> bool
		{
			if (!SourceScript)
			{
				return false;
			}

			const FNiagaraVMExecutableData& ExecutableData = SourceScript->GetVMExecutableData();
			const FString Suffix = TEXT(".") + InputName;
			const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");

			UNiagaraDataInterface* FallbackCurve = nullptr;
			int32 CurveCount = 0;
			TArray<UNiagaraDataInterface*> RapidInterfaces;
			GatherRapidIterationDataInterfaces(SourceScript->RapidIterationParameters, RapidInterfaces);
			TArray<UNiagaraDataInterface*> ExecParamInterfaces;
			GatherRapidIterationDataInterfacesFromParameters(ExecutableData.Parameters, ExecParamInterfaces);
			TArray<UNiagaraDataInterface*> ScriptInterfaces;
			GatherDataInterfacesFromObject(SourceScript, ScriptInterfaces);
			AddDebugField(InputsObject, TEXT("_DebugExecDIInfoCount"), FString::FromInt(ExecutableData.DataInterfaceInfo.Num()));
			AddDebugField(InputsObject, TEXT("_DebugRapidInterfacesCount"), FString::FromInt(RapidInterfaces.Num()));

			for (int32 Index = 0; Index < ExecutableData.DataInterfaceInfo.Num(); ++Index)
			{
				const auto& Info = ExecutableData.DataInterfaceInfo[Index];
				const UScriptStruct* InfoStruct = Info.StaticStruct();
				if (!InfoStruct)
				{
					continue;
				}

				FString DIName;
				if (const FNameProperty* NameProp = FindFProperty<FNameProperty>(InfoStruct, TEXT("Name")))
				{
					DIName = NameProp->GetPropertyValue_InContainer(&Info).ToString();
				}
				else if (const FStrProperty* StrProp = FindFProperty<FStrProperty>(InfoStruct, TEXT("Name")))
				{
					DIName = StrProp->GetPropertyValue_InContainer(&Info);
				}

				if (DIName.IsEmpty())
				{
					continue;
				}

				const bool bMatches = (DIName.EndsWith(Suffix) && DIName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart));
				if (!bMatches)
				{
					continue;
				}

				UNiagaraDataInterface* DataInterface = nullptr;
				static const FName DataInterfaceNames[] = {
					TEXT("DataInterface"),
					TEXT("DataInterfacePtr"),
					TEXT("DataInterfaceObject"),
					TEXT("DataInterfaceInstance")
				};

				for (const FName& PropName : DataInterfaceNames)
				{
					if (const FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(InfoStruct, PropName))
					{
						DataInterface = Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue_InContainer(&Info));
						if (DataInterface)
						{
							break;
						}
					}
				}

				if (!DataInterface)
				{
					for (TFieldIterator<FProperty> It(InfoStruct); It; ++It)
					{
						const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(*It);
						if (!ObjProp || !ObjProp->PropertyClass || !ObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
						{
							continue;
						}

						DataInterface = Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue_InContainer(&Info));
						if (DataInterface)
						{
							break;
						}
					}
				}

				if (!DataInterface && RapidInterfaces.IsValidIndex(Index))
				{
					DataInterface = RapidInterfaces[Index];
				}

				if (!DataInterface && RapidInterfaces.Num() == 1)
				{
					DataInterface = RapidInterfaces[0];
				}

				if (!DataInterface && ExecParamInterfaces.IsValidIndex(Index))
				{
					DataInterface = ExecParamInterfaces[Index];
				}

				if (!DataInterface && ExecParamInterfaces.Num() == 1)
				{
					DataInterface = ExecParamInterfaces[0];
				}

				if (!DataInterface && ScriptInterfaces.Num() > 0)
				{
					FString LastToken;
					DIName.Split(TEXT("."), nullptr, &LastToken, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

					for (UNiagaraDataInterface* Interface : ScriptInterfaces)
					{
						if (!Interface)
						{
							continue;
						}

						const FString InterfaceName = Interface->GetName();
						if (!LastToken.IsEmpty() && InterfaceName.Contains(LastToken))
						{
							DataInterface = Interface;
							break;
						}
					}
				}

				if (!DataInterface)
				{
					int32 ScriptCurveCount = 0;
					UNiagaraDataInterface* ScriptFallbackCurve = nullptr;
					for (UNiagaraDataInterface* Interface : ScriptInterfaces)
					{
						if (!Interface)
						{
							continue;
						}

						if (Cast<UNiagaraDataInterfaceCurve>(Interface) || Cast<UNiagaraDataInterfaceColorCurve>(Interface))
						{
							++ScriptCurveCount;
							if (!ScriptFallbackCurve)
							{
								ScriptFallbackCurve = Interface;
							}
						}
					}

					const bool bScriptHintedInput = InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
						|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
					if (bScriptHintedInput && ScriptCurveCount == 1 && ScriptFallbackCurve)
					{
						DataInterface = ScriptFallbackCurve;
					}
				}

				if (!DataInterface && FallbackDataInterfaces.Num() > 0)
				{
					for (UNiagaraDataInterface* Interface : FallbackDataInterfaces)
					{
						if (!Interface)
						{
							continue;
						}

						const FString InterfaceName = Interface->GetName();
						if (!DIName.IsEmpty() && (InterfaceName.Contains(DIName) || DIName.Contains(InterfaceName)))
						{
							DataInterface = Interface;
							break;
						}
					}

					if (!DataInterface)
					{
						int32 FallbackCurveCount = 0;
						UNiagaraDataInterface* FallbackCurveDI = nullptr;
						for (UNiagaraDataInterface* Interface : FallbackDataInterfaces)
						{
							if (!Interface)
							{
								continue;
							}

							if (Cast<UNiagaraDataInterfaceCurve>(Interface) || Cast<UNiagaraDataInterfaceColorCurve>(Interface))
							{
								++FallbackCurveCount;
								if (!FallbackCurveDI)
								{
									FallbackCurveDI = Interface;
								}
							}
						}

						const bool bHintedInput = InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
							|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
						if (bHintedInput && FallbackCurveCount == 1 && FallbackCurveDI)
						{
							DataInterface = FallbackCurveDI;
						}
					}
				}

				if (!DataInterface && GraphCurveInterfaces.Num() > 0)
				{
					FString LastToken;
					DIName.Split(TEXT("."), nullptr, &LastToken, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

					for (UNiagaraDataInterface* Interface : GraphCurveInterfaces)
					{
						if (!Interface)
						{
							continue;
						}

						const FString InterfaceName = Interface->GetName();
						if (!LastToken.IsEmpty() && InterfaceName.Contains(LastToken))
						{
							DataInterface = Interface;
							break;
						}
					}
				}

				if (!DataInterface && GraphCurveInterfaces.Num() == 1)
				{
					const bool bHintedInput = InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
						|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
					if (bHintedInput)
					{
						DataInterface = GraphCurveInterfaces[0];
					}
				}

				if (!DataInterface)
				{
					continue;
				}

				if (Cast<UNiagaraDataInterfaceCurve>(DataInterface) || Cast<UNiagaraDataInterfaceColorCurve>(DataInterface))
				{
					++CurveCount;
					if (!FallbackCurve)
					{
						FallbackCurve = DataInterface;
					}
				}

				if (const UNiagaraDataInterfaceCurve* FloatCurve = Cast<UNiagaraDataInterfaceCurve>(DataInterface))
				{
					TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
					CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(FloatCurve));
					CurveObject->SetArrayField(TEXT("Keys"), BuildCurveKeysArray(FloatCurve->Curve));
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}

				if (const UNiagaraDataInterfaceColorCurve* ColorCurve = Cast<UNiagaraDataInterfaceColorCurve>(DataInterface))
				{
					TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
					CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(ColorCurve));

					TSharedPtr<FJsonObject> ChannelsObject = MakeShared<FJsonObject>();
					ChannelsObject->SetArrayField(TEXT("R"), BuildCurveKeysArray(ColorCurve->RedCurve));
					ChannelsObject->SetArrayField(TEXT("G"), BuildCurveKeysArray(ColorCurve->GreenCurve));
					ChannelsObject->SetArrayField(TEXT("B"), BuildCurveKeysArray(ColorCurve->BlueCurve));
					ChannelsObject->SetArrayField(TEXT("A"), BuildCurveKeysArray(ColorCurve->AlphaCurve));
					CurveObject->SetObjectField(TEXT("Channels"), ChannelsObject);

					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			const bool bHintedInput = InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
				|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
			if (bHintedInput && CurveCount == 1 && FallbackCurve)
			{
				if (const UNiagaraDataInterfaceCurve* FloatCurve = Cast<UNiagaraDataInterfaceCurve>(FallbackCurve))
				{
					TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
					CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(FloatCurve));
					CurveObject->SetArrayField(TEXT("Keys"), BuildCurveKeysArray(FloatCurve->Curve));
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}

				if (const UNiagaraDataInterfaceColorCurve* ColorCurve = Cast<UNiagaraDataInterfaceColorCurve>(FallbackCurve))
				{
					TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
					CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(ColorCurve));

					TSharedPtr<FJsonObject> ChannelsObject = MakeShared<FJsonObject>();
					ChannelsObject->SetArrayField(TEXT("R"), BuildCurveKeysArray(ColorCurve->RedCurve));
					ChannelsObject->SetArrayField(TEXT("G"), BuildCurveKeysArray(ColorCurve->GreenCurve));
					ChannelsObject->SetArrayField(TEXT("B"), BuildCurveKeysArray(ColorCurve->BlueCurve));
					ChannelsObject->SetArrayField(TEXT("A"), BuildCurveKeysArray(ColorCurve->AlphaCurve));
					CurveObject->SetObjectField(TEXT("Channels"), ChannelsObject);

					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			return false;
		};

		auto TrySetFromSingleCurveList = [&](const TArray<UNiagaraDataInterface*>& Interfaces) -> bool
		{
			const bool bHintedInput = InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
				|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
			if (!bHintedInput || Interfaces.Num() != 1)
			{
				return false;
			}

			if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(Interfaces[0]))
			{
				TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, SourceScript->RapidIterationParameters, CurveObject);
				InputsObject->SetObjectField(InputName, CurveObject);
				return true;
			}

			return false;
		};

		if (!MatchedVariable)
		{
			// Fallback: function-token segment may differ depending on how the stack item was renamed.
			for (const FNiagaraVariable& Variable : RapidIterationVariables)
			{
				const FString VariableName = Variable.GetName().ToString();
				if (!VariableName.EndsWith(Suffix))
				{
					continue;
				}

				int32 Score = VariableName.Len();
				if (VariableName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
				{
					Score += 1000;
				}

				if (Score > BestScore)
				{
					BestScore = Score;
					MatchedVariable = &Variable;
				}
			}
		}

		if (!MatchedVariable)
		{
			TArray<FString> SuffixVars;
			for (const FNiagaraVariable& Variable : RapidIterationVariables)
			{
				const FString VariableName = Variable.GetName().ToString();
				if (VariableName.EndsWith(Suffix))
				{
					SuffixVars.Add(VariableName);
				}
			}
			AddDebugArrayField(InputsObject, TEXT("_DebugRapidSuffixVars"), SuffixVars);

			if (TrySetRapidIterationDataInterfaceByName(SourceScript->RapidIterationParameters))
			{
				return true;
			}

			if (UNiagaraDataInterface* OuterCurve = SelectOuterCurveInterface(OuterCurveInterfaces, SourceScript->GetPathName(), InputName))
			{
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(OuterCurve))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, SourceScript->RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			if (TrySetFromSingleCurveList(GraphCurveInterfaces))
			{
				return true;
			}

			return TrySetExecutableDataInterfaceByName();
		}

		const FNiagaraParameterStore& RapidIterationParameters = SourceScript->RapidIterationParameters;
		if (RapidIterationParameters.IndexOf(*MatchedVariable) == INDEX_NONE)
		{
			AddDebugField(InputsObject, TEXT("_DebugRapidIndexMissing"), MatchedVariable->GetName().ToString());
			return false;
		}

		const FNiagaraTypeDefinition& InputType = MatchedVariable->GetType();
		if (InputType == FNiagaraTypeDefinition::GetFloatDef())
		{
			const float Value = RapidIterationParameters.GetParameterValue<float>(*MatchedVariable);
			InputsObject->SetNumberField(InputName, Value);
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetIntDef())
		{
			const int32 Value = RapidIterationParameters.GetParameterValue<int32>(*MatchedVariable);
			InputsObject->SetNumberField(InputName, Value);
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetBoolDef())
		{
			const FNiagaraBool Value = RapidIterationParameters.GetParameterValue<FNiagaraBool>(*MatchedVariable);
			const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
			InputsObject->SetBoolField(FieldName, Value.GetValue());
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetVec2Def())
		{
			const FVector2f Value = RapidIterationParameters.GetParameterValue<FVector2f>(*MatchedVariable);
			TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
			VecObject->SetNumberField(TEXT("X"), Value.X);
			VecObject->SetNumberField(TEXT("Y"), Value.Y);
			InputsObject->SetObjectField(InputName, VecObject);
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
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
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
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
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
			return true;
		}

		if (InputType == FNiagaraTypeDefinition::GetColorDef())
		{
			const FLinearColor Value = RapidIterationParameters.GetParameterValue<FLinearColor>(*MatchedVariable);
			TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
			ColorObject->SetNumberField(TEXT("R"), Value.R);
			ColorObject->SetNumberField(TEXT("G"), Value.G);
			ColorObject->SetNumberField(TEXT("B"), Value.B);
			ColorObject->SetNumberField(TEXT("A"), Value.A);
			const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Color"));
			InputsObject->SetObjectField(FieldName, ColorObject);
			AddDebugField(InputsObject, TEXT("_DebugRapidSet"), MatchedVariable->GetName().ToString());
			return true;
		}

		if (InputType.IsDataInterface())
		{
			UNiagaraDataInterface* DataInterface = RapidIterationParameters.GetDataInterface(*MatchedVariable);
			if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DataInterface))
			{
				TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
				InputsObject->SetObjectField(InputName, CurveObject);
				AddDebugField(InputsObject, TEXT("_DebugRapidSetDI"), MatchedVariable->GetName().ToString());
				return true;
			}
		}

		if (UNiagaraDataInterface* OuterCurve = SelectOuterCurveInterface(OuterCurveInterfaces, SourceScript->GetPathName(), InputName))
		{
			if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(OuterCurve))
			{
				TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
				InputsObject->SetObjectField(InputName, CurveObject);
				return true;
			}
		}

		if (TrySetFromSingleCurveList(GraphCurveInterfaces))
		{
			return true;
		}

		return TrySetExecutableDataInterfaceByName();
	}

	TSharedPtr<FJsonObject> BuildCurveKeyObject(const FRichCurveKey& Key)
	{
		TSharedPtr<FJsonObject> KeyObject = MakeShared<FJsonObject>();
		KeyObject->SetNumberField(TEXT("Time"), Key.Time);
		KeyObject->SetNumberField(TEXT("Value"), Key.Value);

		TSharedPtr<FJsonObject> TangentObject = MakeShared<FJsonObject>();
		TangentObject->SetNumberField(TEXT("Arrive"), Key.ArriveTangent);
		TangentObject->SetNumberField(TEXT("Leave"), Key.LeaveTangent);
		KeyObject->SetObjectField(TEXT("Tangent"), TangentObject);

		const UEnum* InterpEnum = StaticEnum<ERichCurveInterpMode>();
		KeyObject->SetStringField(TEXT("InterpMode"),
			InterpEnum ? InterpEnum->GetNameStringByValue(static_cast<int64>(Key.InterpMode)) : TEXT("Unknown"));
		return KeyObject;
	}

	TArray<TSharedPtr<FJsonValue>> BuildCurveKeysArray(const FRichCurve& Curve)
	{
		TArray<TSharedPtr<FJsonValue>> KeysArray;
		for (const FRichCurveKey& Key : Curve.GetConstRefOfKeys())
		{
			KeysArray.Add(MakeShared<FJsonValueObject>(BuildCurveKeyObject(Key)));
		}
		return KeysArray;
	}

	void AddDebugField(const TSharedPtr<FJsonObject>& InputsObject, const FString& Key, const FString& Value)
	{
		static const bool bEnableDebug = true;
		if (!InputsObject.IsValid() || !bEnableDebug)
		{
			return;
		}

		InputsObject->SetStringField(Key, Value);
	}

	void AddDebugArrayField(const TSharedPtr<FJsonObject>& InputsObject, const FString& Key, const TArray<FString>& Values)
	{
		static const bool bEnableDebug = true;
		if (!InputsObject.IsValid() || Values.Num() == 0 || !bEnableDebug)
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}

		if (InputsObject->HasField(Key))
		{
			return;
		}

		InputsObject->SetArrayField(Key, JsonValues);
	}

	FString GetUniqueInputFieldName(const TSharedPtr<FJsonObject>& InputsObject, const FString& InputName, const FString& Suffix)
	{
		if (!InputsObject.IsValid())
		{
			return InputName;
		}

		if (!InputsObject->HasField(InputName))
		{
			return InputName;
		}

		FString Candidate = Suffix.IsEmpty()
			? FString::Printf(TEXT("%s_Alt"), *InputName)
			: FString::Printf(TEXT("%s_%s"), *InputName, *Suffix);
		if (!InputsObject->HasField(Candidate))
		{
			return Candidate;
		}

		int32 Index = 1;
		FString IndexedCandidate = Candidate;
		while (InputsObject->HasField(IndexedCandidate))
		{
			IndexedCandidate = FString::Printf(TEXT("%s_%d"), *Candidate, Index++);
		}

		return IndexedCandidate;
	}

	FString NormalizeInputToken(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (TCHAR Ch : In)
		{
			if (FChar::IsAlnum(Ch))
			{
				Out.AppendChar(FChar::ToLower(Ch));
			}
		}
		return Out;
	}

	bool IsSameInputToken(const FString& A, const FString& B)
	{
		if (A == B)
		{
			return true;
		}

		const FString NA = NormalizeInputToken(A);
		const FString NB = NormalizeInputToken(B);
		return !NA.IsEmpty() && NA == NB;
	}

	bool IsBoolPinType(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return false;
		}

		const UStruct* BoolStruct = FNiagaraTypeDefinition::GetBoolDef().GetStruct();
		if (Pin->PinType.PinSubCategoryObject == BoolStruct)
		{
			return true;
		}

		const FString PinCategory = Pin->PinType.PinCategory.ToString();
		return PinCategory.Equals(TEXT("bool"), ESearchCase::IgnoreCase)
			|| PinCategory.Equals(TEXT("boolean"), ESearchCase::IgnoreCase);
	}

	bool TryReadBoolFromNiagaraVariable(const FNiagaraVariable& Variable, bool& OutValue)
	{
		const FNiagaraTypeDefinition Type = Variable.GetType();
		const FString TypeName = Type.GetName();
		const bool bBoolNamed = TypeName.Contains(TEXT("Bool"), ESearchCase::IgnoreCase);

		const int32 Size = Variable.GetSizeInBytes();
		const uint8* Data = Variable.GetData();
		if (!Data || Size <= 0)
		{
			return false;
		}

		if (bBoolNamed && Size == sizeof(FNiagaraBool))
		{
			const FNiagaraBool* BoolValue = reinterpret_cast<const FNiagaraBool*>(Data);
			OutValue = BoolValue && BoolValue->GetValue();
			return true;
		}

		if (bBoolNamed && Size == sizeof(bool))
		{
			OutValue = (*reinterpret_cast<const bool*>(Data) != 0);
			return true;
		}

		if (bBoolNamed && Size == sizeof(uint8))
		{
			OutValue = (*reinterpret_cast<const uint8*>(Data) != 0);
			return true;
		}

		if (bBoolNamed && Size == sizeof(int32))
		{
			OutValue = (*reinterpret_cast<const int32*>(Data) != 0);
			return true;
		}

		// Fallback for engine variants that do not expose a "Bool" typename.
		if (!bBoolNamed && (Size == sizeof(uint8) || Size == sizeof(bool)))
		{
			OutValue = (*reinterpret_cast<const uint8*>(Data) != 0);
			return true;
		}

		if (bBoolNamed)
		{
			bool bAnyNonZero = false;
			for (int32 Index = 0; Index < Size; ++Index)
			{
				if (Data[Index] != 0)
				{
					bAnyNonZero = true;
					break;
				}
			}
			OutValue = bAnyNonZero;
			return true;
		}

		return false;
	}

	FString GetPinDefaultString(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}

		if (!Pin->DefaultValue.IsEmpty())
		{
			return Pin->DefaultValue;
		}

		if (!Pin->AutogeneratedDefaultValue.IsEmpty())
		{
			return Pin->AutogeneratedDefaultValue;
		}

		const FString TextDefault = Pin->DefaultTextValue.ToString();
		if (!TextDefault.IsEmpty())
		{
			return TextDefault;
		}

		return Pin->GetDefaultAsString();
	}

	bool TrySetBoolFromPin(UEdGraphPin* Pin, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Pin || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		if (!IsBoolPinType(Pin))
		{
			return false;
		}

		const FString DefaultValue = GetPinDefaultString(Pin);
		if (DefaultValue.IsEmpty())
		{
			return false;
		}

		const FString Normalized = DefaultValue.TrimStartAndEnd().ToLower();
		bool bValue = false;
		if (Normalized == TEXT("true") || Normalized == TEXT("1"))
		{
			bValue = true;
		}
		else if (Normalized == TEXT("false") || Normalized == TEXT("0"))
		{
			bValue = false;
		}
		else if (Normalized.Contains(TEXT("true"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			bValue = true;
		}
		else if (Normalized.Contains(TEXT("false"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			bValue = false;
		}
		else if (Normalized.Contains(TEXT("value=1"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			bValue = true;
		}
		else if (Normalized.Contains(TEXT("value=0"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			bValue = false;
		}
		else
		{
			return false;
		}

		const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
		InputsObject->SetBoolField(FieldName, bValue);
		AddDebugField(InputsObject, TEXT("_DebugBoolFromPin"), Pin->PinName.ToString());
		return true;
	}

	bool TrySetDefaultValueFromPin(UEdGraphPin* Pin, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Pin || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		if (InputsObject->HasField(InputName))
		{
			return false;
		}

		if (TrySetBoolFromPin(Pin, InputName, InputsObject))
		{
			return true;
		}

		const FString DefaultValue = GetPinDefaultString(Pin);
		if (DefaultValue.IsEmpty())
		{
			return false;
		}

		const UStruct* PinStruct = Cast<UStruct>(Pin->PinType.PinSubCategoryObject.Get());
		const FString PinCategory = Pin->PinType.PinCategory.ToString();
		const bool bIsFloatCategory =
			PinCategory.Equals(TEXT("float"), ESearchCase::IgnoreCase) ||
			PinCategory.Equals(TEXT("real"), ESearchCase::IgnoreCase);
		const bool bIsIntCategory =
			PinCategory.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
			PinCategory.Equals(TEXT("integer"), ESearchCase::IgnoreCase);
		if (PinStruct == FNiagaraTypeDefinition::GetFloatDef().GetStruct())
		{
			float Value = 0.0f;
			if (LexTryParseString(Value, *DefaultValue))
			{
				InputsObject->SetNumberField(InputName, Value);
				return true;
			}
		}
		else if (PinStruct == FNiagaraTypeDefinition::GetIntDef().GetStruct())
		{
			int32 Value = 0;
			if (LexTryParseString(Value, *DefaultValue))
			{
				InputsObject->SetNumberField(InputName, Value);
				return true;
			}
		}
		else if (bIsFloatCategory)
		{
			float Value = 0.0f;
			if (LexTryParseString(Value, *DefaultValue))
			{
				InputsObject->SetNumberField(InputName, Value);
				return true;
			}
		}
		else if (bIsIntCategory)
		{
			int32 Value = 0;
			if (LexTryParseString(Value, *DefaultValue))
			{
				InputsObject->SetNumberField(InputName, Value);
				return true;
			}
		}
		else if (PinStruct == FNiagaraTypeDefinition::GetVec2Def().GetStruct())
		{
			FVector2D Vec;
			if (Vec.InitFromString(DefaultValue))
			{
				TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
				VecObject->SetNumberField(TEXT("X"), Vec.X);
				VecObject->SetNumberField(TEXT("Y"), Vec.Y);
				InputsObject->SetObjectField(InputName, VecObject);
				return true;
			}
		}
		else if (PinStruct == FNiagaraTypeDefinition::GetVec3Def().GetStruct())
		{
			FVector Vec;
			if (Vec.InitFromString(DefaultValue))
			{
				TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
				VecObject->SetNumberField(TEXT("X"), Vec.X);
				VecObject->SetNumberField(TEXT("Y"), Vec.Y);
				VecObject->SetNumberField(TEXT("Z"), Vec.Z);
				InputsObject->SetObjectField(InputName, VecObject);
				return true;
			}
		}
		else if (PinStruct == FNiagaraTypeDefinition::GetVec4Def().GetStruct())
		{
			FVector4 Vec;
			if (Vec.InitFromString(DefaultValue))
			{
				TSharedPtr<FJsonObject> VecObject = MakeShared<FJsonObject>();
				VecObject->SetNumberField(TEXT("X"), Vec.X);
				VecObject->SetNumberField(TEXT("Y"), Vec.Y);
				VecObject->SetNumberField(TEXT("Z"), Vec.Z);
				VecObject->SetNumberField(TEXT("W"), Vec.W);
				InputsObject->SetObjectField(InputName, VecObject);
				return true;
			}
		}
		else if (PinStruct == FNiagaraTypeDefinition::GetColorDef().GetStruct())
		{
			FLinearColor Color;
			if (Color.InitFromString(DefaultValue))
			{
				TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
				ColorObject->SetNumberField(TEXT("R"), Color.R);
				ColorObject->SetNumberField(TEXT("G"), Color.G);
				ColorObject->SetNumberField(TEXT("B"), Color.B);
				ColorObject->SetNumberField(TEXT("A"), Color.A);
				InputsObject->SetObjectField(InputName, ColorObject);
				return true;
			}
		}

		return false;
	}

	bool TrySetDefaultValueFromFunctionNode(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!FunctionNode || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		if (UEdGraphPin* InputPin = FindFunctionInputPin(FunctionNode, InputName))
		{
			if (TrySetDefaultValueFromPin(InputPin, InputName, InputsObject))
			{
				return true;
			}
		}

		if (UNiagaraGraph* OwnerGraph = Cast<UNiagaraGraph>(FunctionNode->GetGraph()))
		{
			if (UEdGraphPin* GraphSetPin = FindParameterMapSetInputPinInGraph(OwnerGraph, InputName, FunctionNode->GetFunctionName()))
			{
				if (TrySetDefaultValueFromPin(GraphSetPin, InputName, InputsObject))
				{
					return true;
				}
			}
		}

		if (TrySetDefaultValueFromCalledGraphInputs(FunctionNode, InputName, InputsObject))
		{
			return true;
		}

		return false;
	}

	bool TrySetDefaultValueFromCalledGraphInputs(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!FunctionNode || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		UNiagaraGraph* CalledGraph = FunctionNode->GetCalledGraph();
		if (!CalledGraph)
		{
			return false;
		}

		TArray<UNiagaraNodeInput*> InputNodes;
		CalledGraph->GetNodesOfClass(InputNodes);
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (!InputNode)
			{
				continue;
			}

			const FString FullName = InputNode->Input.GetName().ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = FullName.FindLastChar(TEXT('.'), DotIndex)
				? FullName.Mid(DotIndex + 1)
				: FullName;
			if (!IsSameInputToken(SimplifiedName, InputName)
				&& !IsSameInputToken(FullName, InputName)
				&& !NormalizeInputToken(FullName).EndsWith(NormalizeInputToken(InputName)))
			{
				continue;
			}

			const FNiagaraTypeDefinition InputType = InputNode->Input.GetType();
			bool bBoolValue = false;
			if (TryReadBoolFromNiagaraVariable(InputNode->Input, bBoolValue))
			{
				const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
				InputsObject->SetBoolField(FieldName, bBoolValue);
				AddDebugField(InputsObject, TEXT("_DebugDefaultFromCalledGraph"), FullName);
				return true;
			}

			if (InputType == FNiagaraTypeDefinition::GetColorDef())
			{
				const FLinearColor Value = InputNode->Input.GetValue<FLinearColor>();
				TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
				ColorObject->SetNumberField(TEXT("R"), Value.R);
				ColorObject->SetNumberField(TEXT("G"), Value.G);
				ColorObject->SetNumberField(TEXT("B"), Value.B);
				ColorObject->SetNumberField(TEXT("A"), Value.A);
				InputsObject->SetObjectField(InputName, ColorObject);
				AddDebugField(InputsObject, TEXT("_DebugDefaultFromCalledGraph"), FullName);
				return true;
			}
		}

		return false;
	}

	bool TrySetDefaultValueFromParameterMapGetNode(UEdGraphNode* GetNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!GetNode || InputName.IsEmpty() || !InputsObject.IsValid())
		{
			return false;
		}

		TArray<UEdGraphPin*> ValueOutputPins;
		TArray<UEdGraphPin*> ValueInputPins;
		for (UEdGraphPin* Pin : GetNode->Pins)
		{
			if (!Pin || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			if (Pin->Direction == EGPD_Output)
			{
				ValueOutputPins.Add(Pin);
			}
			else if (Pin->Direction == EGPD_Input)
			{
				ValueInputPins.Add(Pin);
			}
		}

		int32 OutputIndex = INDEX_NONE;
		for (int32 Index = 0; Index < ValueOutputPins.Num(); ++Index)
		{
			const FString PinName = ValueOutputPins[Index]->PinName.ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = PinName.FindLastChar(TEXT('.'), DotIndex) ? PinName.Mid(DotIndex + 1) : PinName;
			if (IsSameInputToken(SimplifiedName, InputName) || IsSameInputToken(PinName, InputName))
			{
				OutputIndex = Index;
				break;
			}
		}

		if (OutputIndex == INDEX_NONE || !ValueInputPins.IsValidIndex(OutputIndex))
		{
			return false;
		}

		UEdGraphPin* DefaultPin = ValueInputPins[OutputIndex];
		if (TrySetDefaultValueFromPin(DefaultPin, InputName, InputsObject))
		{
			AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("ParameterMapGetIndexedInput"));
			AddDebugField(InputsObject, TEXT("_DebugParameterMapGetMatchedPin"), DefaultPin->PinName.ToString());
			return true;
		}

		return false;
	}

	bool TrySetDefaultValueFromModuleScript(UNiagaraScript* ModuleScript, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!ModuleScript || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("InvalidArgs"));
			return false;
		}

		UNiagaraScriptSource* ModuleSource = Cast<UNiagaraScriptSource>(ModuleScript->GetLatestSource());
		if (!ModuleSource || !ModuleSource->NodeGraph)
		{
			AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("NoModuleSourceOrGraph"));
			return false;
		}
		AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("Begin"));
		AddDebugModuleGraphValuePins(ModuleSource->NodeGraph, InputsObject);

		for (UEdGraphNode* Node : ModuleSource->NodeGraph->Nodes)
		{
			if (!Node || Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapSet"))
			{
				continue;
			}

			UEdGraphPin* Pin = FindParameterMapSetInputPin(Node, InputName, FString());
			if (!Pin)
			{
				continue;
			}

			if (TrySetDefaultValueFromPin(Pin, InputName, InputsObject))
			{
				AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("ParameterMapSet"));
				return true;
			}
		}

		for (UEdGraphNode* Node : ModuleSource->NodeGraph->Nodes)
		{
			if (!Node || Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapGet"))
			{
				continue;
			}

			if (TrySetDefaultValueFromParameterMapGetNode(Node, InputName, InputsObject))
			{
				AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("ParameterMapGetIndexedInput"));
				return true;
			}

			UEdGraphPin* Pin = FindParameterMapGetValuePin(Node, InputName);
			if (!Pin)
			{
				continue;
			}

			if (TrySetDefaultValueFromPin(Pin, InputName, InputsObject))
			{
				AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("ParameterMapGet"));
				AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("ParameterMapGet"));
				return true;
			}
		}

		TArray<UNiagaraNodeInput*> InputNodes;
		ModuleSource->NodeGraph->GetNodesOfClass(InputNodes);
		TArray<TPair<FString, bool>> BoolCandidates;
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (!InputNode)
			{
				continue;
			}

			const FString FullName = InputNode->Input.GetName().ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = FullName.FindLastChar(TEXT('.'), DotIndex)
				? FullName.Mid(DotIndex + 1)
				: FullName;
			if (!IsSameInputToken(SimplifiedName, InputName)
				&& !IsSameInputToken(FullName, InputName)
				&& !NormalizeInputToken(FullName).EndsWith(NormalizeInputToken(InputName)))
			{
				continue;
			}

			const FNiagaraTypeDefinition InputType = InputNode->Input.GetType();
			bool bBoolValue = false;
			if (TryReadBoolFromNiagaraVariable(InputNode->Input, bBoolValue))
			{
				BoolCandidates.Add(TPair<FString, bool>(FullName, bBoolValue));

				const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
				if (IsSameInputToken(SimplifiedName, InputName)
					|| IsSameInputToken(FullName, InputName)
					|| NormalizeInputToken(FullName).EndsWith(NormalizeInputToken(InputName)))
				{
					InputsObject->SetBoolField(FieldName, bBoolValue);
					AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("NodeInput"));
					AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("NodeInput"));
					return true;
				}
			}

			if (InputType == FNiagaraTypeDefinition::GetColorDef())
			{
				const FLinearColor Value = InputNode->Input.GetValue<FLinearColor>();
				TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
				ColorObject->SetNumberField(TEXT("R"), Value.R);
				ColorObject->SetNumberField(TEXT("G"), Value.G);
				ColorObject->SetNumberField(TEXT("B"), Value.B);
				ColorObject->SetNumberField(TEXT("A"), Value.A);
				InputsObject->SetObjectField(InputName, ColorObject);
				AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("NodeInput"));
				AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("NodeInput"));
				return true;
			}
		}

		if (BoolCandidates.Num() == 1)
		{
			const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
			InputsObject->SetBoolField(FieldName, BoolCandidates[0].Value);
			AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("NodeInputSingleBoolFallback"));
			AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("NodeInputSingleBoolFallback"));
			return true;
		}

		if (TrySetDefaultValueFromModuleRapidIteration(ModuleScript, InputName, InputsObject))
		{
			AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("ModuleRapidIteration"));
			return true;
		}

		if (TrySetDefaultValueFromGraphMetadata(ModuleSource->NodeGraph, InputName, InputsObject))
		{
			AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("GraphMetadata"));
			return true;
		}

		AddDebugField(InputsObject, TEXT("_DebugModuleDefaultStage"), TEXT("NotFound"));
		return false;
	}

	bool TrySetDefaultFromMetadataStruct(const void* StructPtr, const UStruct* StructType, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject, int32 Depth)
	{
		if (!StructPtr || !StructType || !InputsObject.IsValid() || Depth > 4)
		{
			return false;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropName = Property->GetName();
			const bool bDefaultLike = PropName.Contains(TEXT("Default"), ESearchCase::IgnoreCase) || PropName.Contains(TEXT("Value"), ESearchCase::IgnoreCase);

			if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
			{
				if (!bDefaultLike)
				{
					continue;
				}

				const bool bValue = BoolProp->GetPropertyValue_InContainer(StructPtr);
				const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
				InputsObject->SetBoolField(FieldName, bValue);
				return true;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* InnerPtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
				if (!InnerPtr)
				{
					continue;
				}

				const UScriptStruct* InnerType = StructProp->Struct;
				if (!InnerType)
				{
					continue;
				}

				if (InnerType->GetName().Contains(TEXT("NiagaraVariable")) && bDefaultLike)
				{
					const FNiagaraVariable* Var = reinterpret_cast<const FNiagaraVariable*>(InnerPtr);
					if (Var)
					{
						bool bBoolValue = false;
						if (TryReadBoolFromNiagaraVariable(*Var, bBoolValue))
						{
							const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
							InputsObject->SetBoolField(FieldName, bBoolValue);
							return true;
						}

						if (Var->GetType() == FNiagaraTypeDefinition::GetColorDef())
						{
							const FLinearColor Value = Var->GetValue<FLinearColor>();
							TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
							ColorObject->SetNumberField(TEXT("R"), Value.R);
							ColorObject->SetNumberField(TEXT("G"), Value.G);
							ColorObject->SetNumberField(TEXT("B"), Value.B);
							ColorObject->SetNumberField(TEXT("A"), Value.A);
							InputsObject->SetObjectField(InputName, ColorObject);
							return true;
						}
					}
				}

				if (InnerType == FNiagaraTypeDefinition::GetBoolDef().GetStruct() && bDefaultLike)
				{
					const FNiagaraBool* BoolValue = reinterpret_cast<const FNiagaraBool*>(InnerPtr);
					if (BoolValue)
					{
						const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
						InputsObject->SetBoolField(FieldName, BoolValue->GetValue());
						return true;
					}
				}

				if (InnerType == TBaseStructure<FLinearColor>::Get() && bDefaultLike)
				{
					const FLinearColor* Value = reinterpret_cast<const FLinearColor*>(InnerPtr);
					if (Value)
					{
						TSharedPtr<FJsonObject> ColorObject = MakeShared<FJsonObject>();
						ColorObject->SetNumberField(TEXT("R"), Value->R);
						ColorObject->SetNumberField(TEXT("G"), Value->G);
						ColorObject->SetNumberField(TEXT("B"), Value->B);
						ColorObject->SetNumberField(TEXT("A"), Value->A);
						InputsObject->SetObjectField(InputName, ColorObject);
						return true;
					}
				}

				if (TrySetDefaultFromMetadataStruct(InnerPtr, InnerType, InputName, InputsObject, Depth + 1))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool TrySetDefaultValueFromGraphMetadata(UNiagaraGraph* Graph, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Graph || InputName.IsEmpty() || !InputsObject.IsValid())
		{
			return false;
		}

		const UStruct* GraphStruct = Graph->GetClass();
		if (!GraphStruct)
		{
			return false;
		}

		TArray<FString> MetadataKeyHits;
		TArray<FString> MetadataMapInfo;
		TArray<FString> MetadataMapSizes;
		int32 MetadataMapCount = 0;
		for (TFieldIterator<FProperty> It(GraphStruct); It; ++It)
		{
			const FMapProperty* MapProp = CastField<FMapProperty>(*It);
			if (!MapProp || !MapProp->KeyProp || !MapProp->ValueProp)
			{
				continue;
			}

			const FString PropName = MapProp->GetName();
			if (!PropName.Contains(TEXT("Meta"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			++MetadataMapCount;

			const FStructProperty* ValueStructProp = CastField<FStructProperty>(MapProp->ValueProp);
			const FString KeyTypeName = MapProp->KeyProp ? MapProp->KeyProp->GetClass()->GetName() : TEXT("<null>");
			const FString ValueTypeName = MapProp->ValueProp ? MapProp->ValueProp->GetClass()->GetName() : TEXT("<null>");
			MetadataMapInfo.Add(FString::Printf(TEXT("%s Key=%s Value=%s"), *PropName, *KeyTypeName, *ValueTypeName));
			if (!ValueStructProp)
			{
				continue;
			}

			FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(Graph));
			MetadataMapSizes.Add(FString::Printf(TEXT("%s Num=%d"), *PropName, MapHelper.Num()));
			for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
			{
				if (!MapHelper.IsValidIndex(Index))
				{
					continue;
				}

				const uint8* KeyPtr = MapHelper.GetKeyPtr(Index);
				const uint8* ValuePtr = MapHelper.GetValuePtr(Index);
				if (!KeyPtr || !ValuePtr)
				{
					continue;
				}

				FString VarName;
				if (!TryExtractVariableLikeName(KeyPtr, MapProp->KeyProp, VarName))
				{
					continue;
				}

				MetadataKeyHits.Add(VarName);
				int32 DotIndex = INDEX_NONE;
				const FString LastToken = VarName.FindLastChar(TEXT('.'), DotIndex) ? VarName.Mid(DotIndex + 1) : VarName;
				if (!IsSameInputToken(VarName, InputName) && !IsSameInputToken(LastToken, InputName))
				{
					continue;
				}

				if (TrySetDefaultFromMetadataStruct(ValuePtr, ValueStructProp->Struct, InputName, InputsObject, 0))
				{
					AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("GraphMetadata"));
					AddDebugField(InputsObject, TEXT("_DebugMetadataVar"), VarName);
					return true;
				}
			}
		}

		AddDebugField(InputsObject, TEXT("_DebugGraphMetadataMapCount"), FString::FromInt(MetadataMapCount));
		AddDebugArrayField(InputsObject, TEXT("_DebugGraphMetadataMapInfo"), MetadataMapInfo);
		AddDebugArrayField(InputsObject, TEXT("_DebugGraphMetadataMapSizes"), MetadataMapSizes);
		AddDebugArrayField(InputsObject, TEXT("_DebugGraphMetadataKeys"), MetadataKeyHits);
		return false;
	}

	bool TrySetDefaultValueFromModuleRapidIteration(UNiagaraScript* ModuleScript, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!ModuleScript || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		TArray<FNiagaraVariable> Vars;
		ModuleScript->RapidIterationParameters.GetParameters(Vars);
		if (Vars.Num() == 0)
		{
			return false;
		}

		const FString Suffix = TEXT(".") + InputName;
		const FNiagaraVariable* BestBoolVar = nullptr;
		int32 BestScore = -1;
		for (const FNiagaraVariable& Var : Vars)
		{
			const FString Name = Var.GetName().ToString();
			if (!Name.EndsWith(Suffix))
			{
				continue;
			}

			if (Var.GetType() != FNiagaraTypeDefinition::GetBoolDef())
			{
				continue;
			}

			const int32 Score = Name.Len();
			if (Score > BestScore)
			{
				BestScore = Score;
				BestBoolVar = &Var;
			}
		}

		if (!BestBoolVar || ModuleScript->RapidIterationParameters.IndexOf(*BestBoolVar) == INDEX_NONE)
		{
			return false;
		}

		const FNiagaraBool Value = ModuleScript->RapidIterationParameters.GetParameterValue<FNiagaraBool>(*BestBoolVar);
		const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
		InputsObject->SetBoolField(FieldName, Value.GetValue());
		AddDebugField(InputsObject, TEXT("_DebugDefaultFromModule"), TEXT("ModuleRapidIteration"));
		AddDebugField(InputsObject, TEXT("_DebugModuleRapidSet"), BestBoolVar->GetName().ToString());
		return true;
	}

	UEdGraphPin* FindParameterMapGetValuePin(UEdGraphNode* GetNode, const FString& InputName)
	{
		if (!GetNode || InputName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : GetNode->Pins)
		{
			if (!Pin || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = PinName.FindLastChar(TEXT('.'), DotIndex)
				? PinName.Mid(DotIndex + 1)
				: PinName;
			if (SimplifiedName == InputName
				|| PinName == InputName
				|| PinName.EndsWith(TEXT(".") + InputName)
				|| PinName.Contains(InputName, ESearchCase::IgnoreCase)
				|| IsSameInputToken(SimplifiedName, InputName)
				|| IsSameInputToken(PinName, InputName))
			{
				return Pin;
			}
		}

		return nullptr;
	}

	bool TrySetBoolFromPinNameMatch(UEdGraphNode* Node, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Node || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			FString LastToken = PinName;
			int32 DotIndex = INDEX_NONE;
			if (PinName.FindLastChar(TEXT('.'), DotIndex))
			{
				LastToken = PinName.Mid(DotIndex + 1);
			}

			if (!PinName.Contains(InputName, ESearchCase::IgnoreCase)
				&& !IsSameInputToken(PinName, InputName)
				&& !IsSameInputToken(LastToken, InputName))
			{
				continue;
			}

			if (TrySetBoolFromPin(Pin, InputName, InputsObject))
			{
				return true;
			}
		}

		return false;
	}

	void GatherDataInterfacesFromObject(const UObject* Owner, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!Owner)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
			{
				if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue_InContainer(Owner)))
					{
						OutDataInterfaces.Add(DataInterface);
					}
				}
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
				if (!InnerObjProp || !InnerObjProp->PropertyClass || !InnerObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					continue;
				}

				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ObjPtr = Helper.GetRawPtr(Index);
					if (!ObjPtr)
					{
						continue;
					}

					if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(InnerObjProp->GetObjectPropertyValue(ObjPtr)))
					{
						OutDataInterfaces.Add(DataInterface);
					}
				}
			}
		}
	}

	void GatherStructDebug(const void* StructPtr, const UScriptStruct* StructType, TArray<FString>& OutProps, TArray<FString>& OutObjects, TArray<FString>& OutObjectArrays)
	{
		if (!StructPtr || !StructType)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			OutProps.Add(FString::Printf(TEXT("%s (%s)"), *Property->GetName(), *Property->GetClass()->GetName()));

			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
			{
				UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(StructPtr);
				const FString ObjName = Obj ? Obj->GetName() : TEXT("<null>");
				const FString ObjClass = Obj ? Obj->GetClass()->GetName() : TEXT("<null>");
				OutObjects.Add(FString::Printf(TEXT("%s : %s (%s)"), *Property->GetName(), *ObjName, *ObjClass));
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
				if (!InnerObjProp)
				{
					continue;
				}

				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructPtr));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ObjPtr = Helper.GetRawPtr(Index);
					if (!ObjPtr)
					{
						continue;
					}

					UObject* Obj = InnerObjProp->GetObjectPropertyValue(ObjPtr);
					const FString ObjName = Obj ? Obj->GetName() : TEXT("<null>");
					const FString ObjClass = Obj ? Obj->GetClass()->GetName() : TEXT("<null>");
					OutObjectArrays.Add(FString::Printf(TEXT("%s[%d] : %s (%s)"), *Property->GetName(), Index, *ObjName, *ObjClass));
				}
			}
		}
	}

	void GatherRapidIterationDataInterfacesFromParameters(const FNiagaraParameters& Parameters, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		const UScriptStruct* ParamsStruct = FNiagaraParameters::StaticStruct();
		if (!ParamsStruct)
		{
			return;
		}

		const FStructProperty* StoreProp = FindFProperty<FStructProperty>(ParamsStruct, TEXT("ParameterStore"));
		if (!StoreProp)
		{
			return;
		}

		const void* StorePtr = StoreProp->ContainerPtrToValuePtr<void>(&Parameters);
		if (!StorePtr)
		{
			return;
		}

		const FNiagaraParameterStore* Store = static_cast<const FNiagaraParameterStore*>(StorePtr);
		if (!Store)
		{
			return;
		}

		GatherRapidIterationDataInterfaces(*Store, OutDataInterfaces);
	}

	void FindDataInterfacesInStruct(const void* StructPtr, const UStruct* StructType, const FString& Path, TArray<FString>& OutEntries, int32 Depth)
	{
		if (!StructPtr || !StructType || Depth > 4 || OutEntries.Num() > 256)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropPath = Path.IsEmpty()
				? Property->GetName()
				: FString::Printf(TEXT("%s.%s"), *Path, *Property->GetName());

			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
			{
				if (!ObjProp->PropertyClass || !ObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					continue;
				}

				UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(StructPtr);
				if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
				{
					OutEntries.Add(FString::Printf(TEXT("%s : %s (%s)"), *PropPath, *DataInterface->GetName(), *DataInterface->GetClass()->GetName()));
				}
				continue;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* InnerPtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
				FindDataInterfacesInStruct(InnerPtr, StructProp->Struct, PropPath, OutEntries, Depth + 1);
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

				if (const FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner))
				{
					if (!InnerObjProp->PropertyClass || !InnerObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
					{
						continue;
					}

					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* ObjPtr = Helper.GetRawPtr(Index);
						if (!ObjPtr)
						{
							continue;
						}

						UObject* Obj = InnerObjProp->GetObjectPropertyValue(ObjPtr);
						if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
						{
							OutEntries.Add(FString::Printf(TEXT("%s[%d] : %s (%s)"), *PropPath, Index, *DataInterface->GetName(), *DataInterface->GetClass()->GetName()));
						}
					}

					continue;
				}

				if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* InnerPtr = Helper.GetRawPtr(Index);
						if (!InnerPtr)
						{
							continue;
						}

						const FString ElemPath = FString::Printf(TEXT("%s[%d]"), *PropPath, Index);
						FindDataInterfacesInStruct(InnerPtr, InnerStructProp->Struct, ElemPath, OutEntries, Depth + 1);
					}
				}

				continue;
			}

			if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptMapHelper MapHelper(MapProp, MapPtr);

				if (const FObjectPropertyBase* ValueObjProp = CastField<FObjectPropertyBase>(MapProp->ValueProp))
				{
					if (!ValueObjProp->PropertyClass || !ValueObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
					{
						continue;
					}

					for (int32 Index = 0; Index < MapHelper.Num(); ++Index)
					{
						if (!MapHelper.IsValidIndex(Index))
						{
							continue;
						}

						const uint8* PairPtr = MapHelper.GetPairPtr(Index);
						const void* ValuePtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);
						if (!ValuePtr)
						{
							continue;
						}

						UObject* Obj = ValueObjProp->GetObjectPropertyValue(ValuePtr);
						if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
						{
							OutEntries.Add(FString::Printf(TEXT("%s[%d] : %s (%s)"), *PropPath, Index, *DataInterface->GetName(), *DataInterface->GetClass()->GetName()));
						}
					}

					continue;
				}

				if (const FStructProperty* ValueStructProp = CastField<FStructProperty>(MapProp->ValueProp))
				{
					for (int32 Index = 0; Index < MapHelper.Num(); ++Index)
					{
						if (!MapHelper.IsValidIndex(Index))
						{
							continue;
						}

						const uint8* PairPtr = MapHelper.GetPairPtr(Index);
						const void* ValuePtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);
						if (!ValuePtr)
						{
							continue;
						}

						const FString EntryPath = FString::Printf(TEXT("%s[%d]"), *PropPath, Index);
						FindDataInterfacesInStruct(ValuePtr, ValueStructProp->Struct, EntryPath, OutEntries, Depth + 1);
					}
				}
			}
		}
	}

	void CollectDataInterfacesFromStruct(const void* StructPtr, const UStruct* StructType, TArray<UNiagaraDataInterface*>& OutDataInterfaces, int32 Depth)
	{
		if (!StructPtr || !StructType || Depth > 4 || OutDataInterfaces.Num() > 256)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
			{
				if (!ObjProp->PropertyClass || !ObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					continue;
				}

				UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(StructPtr);
				if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
				{
					OutDataInterfaces.Add(DataInterface);
				}
				continue;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* InnerPtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
				CollectDataInterfacesFromStruct(InnerPtr, StructProp->Struct, OutDataInterfaces, Depth + 1);
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

				if (const FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner))
				{
					if (!InnerObjProp->PropertyClass || !InnerObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
					{
						continue;
					}

					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* ObjPtr = Helper.GetRawPtr(Index);
						if (!ObjPtr)
						{
							continue;
						}

						UObject* Obj = InnerObjProp->GetObjectPropertyValue(ObjPtr);
						if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
						{
							OutDataInterfaces.Add(DataInterface);
						}
					}

					continue;
				}

				if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* InnerPtr = Helper.GetRawPtr(Index);
						if (!InnerPtr)
						{
							continue;
						}

						CollectDataInterfacesFromStruct(InnerPtr, InnerStructProp->Struct, OutDataInterfaces, Depth + 1);
					}
				}

				continue;
			}

			if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptMapHelper MapHelper(MapProp, MapPtr);

				if (const FObjectPropertyBase* ValueObjProp = CastField<FObjectPropertyBase>(MapProp->ValueProp))
				{
					if (!ValueObjProp->PropertyClass || !ValueObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
					{
						continue;
					}

					for (int32 Index = 0; Index < MapHelper.Num(); ++Index)
					{
						if (!MapHelper.IsValidIndex(Index))
						{
							continue;
						}

						const uint8* PairPtr = MapHelper.GetPairPtr(Index);
						const void* ValuePtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);
						if (!ValuePtr)
						{
							continue;
						}

						UObject* Obj = ValueObjProp->GetObjectPropertyValue(ValuePtr);
						if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Obj))
						{
							OutDataInterfaces.Add(DataInterface);
						}
					}

					continue;
				}

				if (const FStructProperty* ValueStructProp = CastField<FStructProperty>(MapProp->ValueProp))
				{
					for (int32 Index = 0; Index < MapHelper.Num(); ++Index)
					{
						if (!MapHelper.IsValidIndex(Index))
						{
							continue;
						}

						const uint8* PairPtr = MapHelper.GetPairPtr(Index);
						const void* ValuePtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);
						if (!ValuePtr)
						{
							continue;
						}

						CollectDataInterfacesFromStruct(ValuePtr, ValueStructProp->Struct, OutDataInterfaces, Depth + 1);
					}
				}
			}
		}
	}

	void CollectCurveDataInterfacesFromGraph(UNiagaraGraph* Graph, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (UNiagaraDataInterface* DataInterface = FindCurveDataInterfaceFromNode(Node))
			{
				if (HasCurveDataInterface(DataInterface))
				{
					OutDataInterfaces.AddUnique(DataInterface);
				}
			}
		}
	}

	void CollectCurveDataInterfacesFromFunctionCall(UNiagaraNodeFunctionCall* FunctionNode, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!FunctionNode)
		{
			return;
		}

		UNiagaraGraph* CalledGraph = FunctionNode->GetCalledGraph();
		if (!CalledGraph)
		{
			return;
		}

		TArray<UNiagaraNodeInput*> InputNodes;
		CalledGraph->GetNodesOfClass(InputNodes);

		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (!InputNode)
			{
				continue;
			}

			const FObjectProperty* DataInterfaceProperty = FindFProperty<FObjectProperty>(InputNode->GetClass(), TEXT("DataInterface"));
			if (!DataInterfaceProperty)
			{
				continue;
			}

			UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(
				DataInterfaceProperty->GetObjectPropertyValue_InContainer(InputNode));
			if (!DataInterface)
			{
				continue;
			}

			if (HasCurveDataInterface(DataInterface))
			{
				OutDataInterfaces.AddUnique(DataInterface);
			}
		}
	}

	void CollectCurveDataInterfacesFromDynamicInputs(UNiagaraGraph* Graph, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!Graph)
		{
			return;
		}

		TArray<UNiagaraNodeFunctionCall*> FunctionNodes;
		Graph->GetNodesOfClass(FunctionNodes);

		for (UNiagaraNodeFunctionCall* FunctionNode : FunctionNodes)
		{
			if (!FunctionNode || !FunctionNode->FunctionScript)
			{
				continue;
			}

			if (FunctionNode->FunctionScript->GetUsage() != ENiagaraScriptUsage::DynamicInput)
			{
				continue;
			}

			CollectCurveDataInterfacesFromFunctionCall(FunctionNode, OutDataInterfaces);
		}
	}

	void CollectDynamicInputNodeDebug(UNiagaraNodeFunctionCall* FunctionNode, TArray<FString>& OutEntries)
	{
		if (!FunctionNode)
		{
			return;
		}

		UNiagaraGraph* CalledGraph = FunctionNode->GetCalledGraph();
		if (!CalledGraph)
		{
			return;
		}

		TArray<UNiagaraNodeInput*> InputNodes;
		CalledGraph->GetNodesOfClass(InputNodes);

		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (!InputNode)
			{
				continue;
			}

			const FString InputName = InputNode->Input.GetName().ToString();
			const FString InputType = InputNode->Input.GetType().GetName();
			OutEntries.Add(FString::Printf(TEXT("%s : %s"), *InputName, *InputType));
		}
	}

	void GatherInputNamesFromFunctionCallPins(UNiagaraNodeFunctionCall* FunctionNode, TSet<FString>& OutInputNames)
	{
		if (!FunctionNode)
		{
			return;
		}

		for (UEdGraphPin* Pin : FunctionNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			int32 DotIndex = INDEX_NONE;
			const FString InputName = PinName.FindLastChar(TEXT('.'), DotIndex)
				? PinName.Mid(DotIndex + 1)
				: PinName;
			OutInputNames.Add(InputName);
		}
	}

	void GatherInputNamesFromRapidIteration(const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, TSet<FString>& OutInputNames)
	{
		if (FunctionName.IsEmpty())
		{
			return;
		}

		const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");
		for (const FNiagaraVariable& Variable : RapidIterationVariables)
		{
			const FString VarName = Variable.GetName().ToString();
			if (!VarName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
			{
				continue;
			}

			int32 DotIndex = INDEX_NONE;
			const FString InputName = VarName.FindLastChar(TEXT('.'), DotIndex)
				? VarName.Mid(DotIndex + 1)
				: VarName;
			if (!InputName.IsEmpty())
			{
				OutInputNames.Add(InputName);
			}
		}
	}

	bool TrySetStaticSwitchValue(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!FunctionNode || !InputsObject.IsValid() || InputName.IsEmpty())
		{
			return false;
		}

		const FString Suffix = TEXT(".") + InputName;
		const FString FunctionToken = TEXT(".") + FunctionNode->GetFunctionName() + TEXT(".");

		const FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(FunctionNode->GetClass(), TEXT("PropagatedStaticSwitchParameters"));
		const FStructProperty* ElemStructProp = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (!ArrayProp || !ElemStructProp)
		{
			return false;
		}

		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(FunctionNode));
		const UStruct* ElemStruct = ElemStructProp->Struct;
		TArray<FString> SwitchDebug;
		bool bFoundAnyCandidate = false;
		FString BestMatchVarName;
		bool bBestMatchValue = false;
		int32 BestScore = -1;
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const void* ElemPtr = Helper.GetRawPtr(Index);
			if (!ElemPtr || !ElemStruct)
			{
				continue;
			}

			const FNiagaraVariableBase* VarBase = nullptr;
			bool bValue = false;
			bool bHasValue = false;

			for (TFieldIterator<FProperty> It(ElemStruct); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property)
				{
					continue;
				}

				if (!VarBase)
				{
					if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
					{
						const UScriptStruct* StructType = StructProp->Struct;
						if (StructType && (StructType->GetName().Equals(TEXT("NiagaraVariable")) || StructType->GetName().Equals(TEXT("NiagaraVariableBase"))))
						{
							VarBase = StructProp->ContainerPtrToValuePtr<FNiagaraVariableBase>(ElemPtr);
							continue;
						}
					}
				}

				if (!bHasValue)
				{
					if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
					{
						bValue = BoolProp->GetPropertyValue_InContainer(ElemPtr);
						bHasValue = true;
						continue;
					}

					if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
					{
						const UScriptStruct* StructType = StructProp->Struct;
						if (StructType && StructType->GetName().Equals(TEXT("NiagaraBool")))
						{
							const FNiagaraBool* BoolValue = StructProp->ContainerPtrToValuePtr<FNiagaraBool>(ElemPtr);
							if (BoolValue)
							{
								bValue = BoolValue->GetValue();
								bHasValue = true;
								continue;
							}
						}
					}
				}
			}

			if (!VarBase || !bHasValue)
			{
				continue;
			}

			const FString VarName = VarBase->GetName().ToString();
			if (VarName.IsEmpty())
			{
				continue;
			}

			SwitchDebug.Add(FString::Printf(TEXT("%s=%s"), *VarName, bValue ? TEXT("true") : TEXT("false")));

			int32 Score = 0;
			if (VarName.EndsWith(Suffix))
			{
				Score += 100;
			}
			else if (VarName.Contains(InputName, ESearchCase::IgnoreCase))
			{
				Score += 50;
			}
			else
			{
				continue;
			}

			if (VarName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
			{
				Score += 20;
			}

			if (Score > BestScore)
			{
				BestScore = Score;
				BestMatchVarName = VarName;
				bBestMatchValue = bValue;
				bFoundAnyCandidate = true;
			}
		}

		if (bFoundAnyCandidate)
		{
			const FString FieldName = GetUniqueInputFieldName(InputsObject, InputName, TEXT("Bool"));
			InputsObject->SetBoolField(FieldName, bBestMatchValue);
			AddDebugField(InputsObject, TEXT("_DebugStaticSwitchSet"), BestMatchVarName);
			return true;
		}

		AddDebugArrayField(InputsObject, TEXT("_DebugStaticSwitchVars"), SwitchDebug);
		return false;
	}

	struct FCurveChannel
	{
		FString Name;
		const FRichCurve* Curve = nullptr;
	};

	bool ShouldSkipCurveChannelName(const FString& Name)
	{
		const FString LowerName = Name.ToLower();
		return LowerName.Contains(TEXT("cooked")) || LowerName.Contains(TEXT("cache")) || LowerName.Contains(TEXT("baked"));
	}

	void CollectCurveChannelsFromStruct(const void* StructPtr, const UStruct* StructType, int32 Depth, TArray<FCurveChannel>& OutChannels)
	{
		if (!StructPtr || !StructType || Depth > 2)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				if (ShouldSkipCurveChannelName(StructProp->GetName()))
				{
					continue;
				}

				const void* InnerPtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
				if (!InnerPtr)
				{
					continue;
				}

				if (StructProp->Struct == FRichCurve::StaticStruct())
				{
					const FRichCurve* Curve = StructProp->ContainerPtrToValuePtr<FRichCurve>(StructPtr);
					if (Curve)
					{
						OutChannels.Add({StructProp->GetName(), Curve});
					}
					continue;
				}

				CollectCurveChannelsFromStruct(InnerPtr, StructProp->Struct, Depth + 1, OutChannels);
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				if (ShouldSkipCurveChannelName(ArrayProp->GetName()))
				{
					continue;
				}

				const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
				if (!InnerStructProp)
				{
					continue;
				}

				const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ElemPtr = Helper.GetRawPtr(Index);
					if (!ElemPtr)
					{
						continue;
					}

					if (InnerStructProp->Struct == FRichCurve::StaticStruct())
					{
						const FRichCurve* Curve = InnerStructProp->ContainerPtrToValuePtr<FRichCurve>(ElemPtr);
						if (Curve)
						{
							OutChannels.Add({FString::Printf(TEXT("%s[%d]"), *ArrayProp->GetName(), Index), Curve});
						}
						continue;
					}

					CollectCurveChannelsFromStruct(ElemPtr, InnerStructProp->Struct, Depth + 1, OutChannels);
				}
				continue;
			}

			if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				if (ShouldSkipCurveChannelName(MapProp->GetName()))
				{
					continue;
				}

				const FStructProperty* ValueStructProp = CastField<FStructProperty>(MapProp->ValueProp);
				if (!ValueStructProp)
				{
					continue;
				}

				const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptMapHelper Helper(MapProp, MapPtr);
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					if (!Helper.IsValidIndex(Index))
					{
						continue;
					}

					const uint8* PairPtr = Helper.GetPairPtr(Index);
					const void* ValuePtr = MapProp->ValueProp->ContainerPtrToValuePtr<void>(PairPtr);
					if (!ValuePtr)
					{
						continue;
					}

					if (ValueStructProp->Struct == FRichCurve::StaticStruct())
					{
						const FRichCurve* Curve = ValueStructProp->ContainerPtrToValuePtr<FRichCurve>(ValuePtr);
						if (Curve)
						{
							OutChannels.Add({FString::Printf(TEXT("%s[%d]"), *MapProp->GetName(), Index), Curve});
						}
						continue;
					}

					CollectCurveChannelsFromStruct(ValuePtr, ValueStructProp->Struct, Depth + 1, OutChannels);
				}
			}
		}
	}

	bool GetCurveChannelsFromDataInterface(UNiagaraDataInterface* DataInterface, TArray<FCurveChannel>& OutChannels)
	{
		if (!DataInterface)
		{
			return false;
		}

		CollectCurveChannelsFromStruct(DataInterface, DataInterface->GetClass(), 0, OutChannels);
		return OutChannels.Num() > 0;
	}

	bool HasCurveDataInterface(UNiagaraDataInterface* DataInterface)
	{
		if (!DataInterface)
		{
			return false;
		}

		if (Cast<UNiagaraDataInterfaceCurve>(DataInterface) || Cast<UNiagaraDataInterfaceColorCurve>(DataInterface))
		{
			return true;
		}

		TArray<FCurveChannel> Channels;
		return GetCurveChannelsFromDataInterface(DataInterface, Channels);
	}

	int32 CountCurveKeysForDataInterface(UNiagaraDataInterface* DataInterface)
	{
		TArray<FCurveChannel> Channels;
		if (!GetCurveChannelsFromDataInterface(DataInterface, Channels))
		{
			return 0;
		}

		int32 TotalKeys = 0;
		for (const FCurveChannel& Channel : Channels)
		{
			if (Channel.Curve)
			{
				TotalKeys += Channel.Curve->GetConstRefOfKeys().Num();
			}
		}

		return TotalKeys;
	}

	UNiagaraDataInterface* SelectOuterCurveInterfaceForDynamicInput(const TArray<UNiagaraDataInterface*>& Interfaces,
		const FString& ScriptPath, const FString& DynamicInputName, const FString& InputName)
	{
		UNiagaraDataInterface* BestInterface = nullptr;
		int32 BestScore = -1;

		for (UNiagaraDataInterface* Interface : Interfaces)
		{
			if (!Interface || !HasCurveDataInterface(Interface))
			{
				continue;
			}

			const FString Path = Interface->GetPathName();
			int32 Score = 0;
			if (!ScriptPath.IsEmpty() && Path.Contains(ScriptPath))
			{
				Score += 100;
			}
			if (!DynamicInputName.IsEmpty() && Path.Contains(DynamicInputName))
			{
				Score += 50;
			}
			if (!InputName.IsEmpty() && Path.Contains(InputName))
			{
				Score += 10;
			}

			const int32 KeyCount = CountCurveKeysForDataInterface(Interface);
			Score += KeyCount;

			if (Score > BestScore)
			{
				BestScore = Score;
				BestInterface = Interface;
			}
		}

		return BestInterface;
	}

	TSharedPtr<FJsonObject> BuildCurveObjectFromDataInterface(UNiagaraDataInterface* DataInterface)
	{
		if (!DataInterface)
		{
			return nullptr;
		}

		if (const UNiagaraDataInterfaceCurve* FloatCurve = Cast<UNiagaraDataInterfaceCurve>(DataInterface))
		{
			TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
			if (!TrySetCurveScaleFieldFromDataInterface(FloatCurve, CurveObject))
			{
				CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(FloatCurve));
			}
			CurveObject->SetArrayField(TEXT("Keys"), BuildCurveKeysArray(FloatCurve->Curve));
			return CurveObject;
		}

		if (const UNiagaraDataInterfaceColorCurve* ColorCurve = Cast<UNiagaraDataInterfaceColorCurve>(DataInterface))
		{
			TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
			if (!TrySetCurveScaleFieldFromDataInterface(ColorCurve, CurveObject))
			{
				CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(ColorCurve));
			}

			TSharedPtr<FJsonObject> ChannelsObject = MakeShared<FJsonObject>();
			ChannelsObject->SetArrayField(TEXT("R"), BuildCurveKeysArray(ColorCurve->RedCurve));
			ChannelsObject->SetArrayField(TEXT("G"), BuildCurveKeysArray(ColorCurve->GreenCurve));
			ChannelsObject->SetArrayField(TEXT("B"), BuildCurveKeysArray(ColorCurve->BlueCurve));
			ChannelsObject->SetArrayField(TEXT("A"), BuildCurveKeysArray(ColorCurve->AlphaCurve));
			CurveObject->SetObjectField(TEXT("Channels"), ChannelsObject);
			return CurveObject;
		}

		TArray<FCurveChannel> Channels;
		if (!GetCurveChannelsFromDataInterface(DataInterface, Channels))
		{
			return nullptr;
		}

		auto NormalizeChannelName = [](const FString& Name) -> FString
		{
			const FString LowerName = Name.ToLower();
			if (LowerName.Contains(TEXT("cooked")) || LowerName.Contains(TEXT("cache")) || LowerName.Contains(TEXT("baked")))
			{
				return TEXT("");
			}
			if (LowerName == TEXT("xcurve") || LowerName == TEXT("x"))
			{
				return TEXT("X");
			}
			if (LowerName == TEXT("ycurve") || LowerName == TEXT("y"))
			{
				return TEXT("Y");
			}
			if (LowerName == TEXT("zcurve") || LowerName == TEXT("z"))
			{
				return TEXT("Z");
			}
			if (LowerName == TEXT("wcurve") || LowerName == TEXT("w"))
			{
				return TEXT("W");
			}
			if (LowerName == TEXT("redcurve") || LowerName == TEXT("r"))
			{
				return TEXT("R");
			}
			if (LowerName == TEXT("greencurve") || LowerName == TEXT("g"))
			{
				return TEXT("G");
			}
			if (LowerName == TEXT("bluecurve") || LowerName == TEXT("b"))
			{
				return TEXT("B");
			}
			if (LowerName == TEXT("alphacurve") || LowerName == TEXT("a"))
			{
				return TEXT("A");
			}
			if (LowerName == TEXT("curve"))
			{
				return TEXT("Curve");
			}

			return Name;
		};

		TSharedPtr<FJsonObject> CurveObject = MakeShared<FJsonObject>();
		if (!TrySetCurveScaleFieldFromDataInterface(DataInterface, CurveObject))
		{
			CurveObject->SetNumberField(TEXT("CurveScale"), ExtractCurveScale(DataInterface));
		}

		if (Channels.Num() == 1)
		{
			const FString ChannelName = NormalizeChannelName(Channels[0].Name);
			if (ChannelName.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
			{
				CurveObject->SetArrayField(TEXT("Keys"), BuildCurveKeysArray(*Channels[0].Curve));
				return CurveObject;
			}
		}

		const FString ClassName = DataInterface->GetClass()->GetName();
		const bool bIsVector = ClassName.Contains(TEXT("Vector"), ESearchCase::IgnoreCase);
		const bool bIsColor = ClassName.Contains(TEXT("Color"), ESearchCase::IgnoreCase);
		bool bAllGeneric = true;
		for (const FCurveChannel& Channel : Channels)
		{
			const FString ChannelName = NormalizeChannelName(Channel.Name);
			if (!ChannelName.IsEmpty() && !ChannelName.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
			{
				bAllGeneric = false;
				break;
			}
		}

		TSharedPtr<FJsonObject> ChannelsObject = MakeShared<FJsonObject>();
		TSet<FString> UsedNames;
		for (int32 Index = 0; Index < Channels.Num(); ++Index)
		{
			FString ChannelName = NormalizeChannelName(Channels[Index].Name);
			if (ChannelName.IsEmpty())
			{
				continue;
			}

			if (bAllGeneric)
			{
				if (bIsVector)
				{
					static const TCHAR* VectorNames[] = {TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W")};
					if (Index < UE_ARRAY_COUNT(VectorNames))
					{
						ChannelName = VectorNames[Index];
					}
				}
				else if (bIsColor)
				{
					static const TCHAR* ColorNames[] = {TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")};
					if (Index < UE_ARRAY_COUNT(ColorNames))
					{
						ChannelName = ColorNames[Index];
					}
				}
			}

			if (ChannelName.IsEmpty() || (ChannelName.Equals(TEXT("Curve"), ESearchCase::IgnoreCase) && Channels.Num() > 1))
			{
				ChannelName = Channels[Index].Name;
			}

			FString UniqueName = ChannelName;
			int32 Suffix = 1;
			while (UsedNames.Contains(UniqueName))
			{
				UniqueName = FString::Printf(TEXT("%s_%d"), *ChannelName, Suffix++);
			}

			UsedNames.Add(UniqueName);
			ChannelsObject->SetArrayField(UniqueName, BuildCurveKeysArray(*Channels[Index].Curve));
		}

		CurveObject->SetObjectField(TEXT("Channels"), ChannelsObject);
		return CurveObject;
	}

	bool TrySetCurvesFromFunctionCallNode(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject,
		const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, const FNiagaraParameterStore& RapidIterationParameters,
		const TArray<UNiagaraDataInterface*>& OuterCurveInterfaces, const FString& SourceScriptPath)
	{
		if (!FunctionNode || !InputsObject.IsValid())
		{
			return false;
		}

		if (FunctionNode->FunctionScript && FunctionNode->FunctionScript->GetUsage() == ENiagaraScriptUsage::DynamicInput)
		{
			const FString DynamicInputName = FunctionNode->GetFunctionName();
			if (UNiagaraDataInterface* OuterCurve = SelectOuterCurveInterfaceForDynamicInput(OuterCurveInterfaces, SourceScriptPath, DynamicInputName, InputName))
			{
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(OuterCurve))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			TArray<UNiagaraDataInterface*> DynamicCurveInterfaces;
			CollectCurveDataInterfacesFromFunctionCall(FunctionNode, DynamicCurveInterfaces);
			AddDebugField(InputsObject, TEXT("_DebugDynamicInputCurveCount"), FString::FromInt(DynamicCurveInterfaces.Num()));
			TArray<FString> DynamicCurveClasses;
			for (UNiagaraDataInterface* Interface : DynamicCurveInterfaces)
			{
				if (Interface)
				{
					DynamicCurveClasses.Add(Interface->GetClass()->GetName());
				}
			}
			AddDebugArrayField(InputsObject, TEXT("_DebugDynamicInputCurveClasses"), DynamicCurveClasses);
			if (DynamicCurveInterfaces.Num() == 1)
			{
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DynamicCurveInterfaces[0]))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}
		}

		TMap<FString, TSharedPtr<FJsonObject>> CurveMap;
		for (UEdGraphPin* Pin : FunctionNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Pin->DefaultObject);
			if (!DataInterface)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin)
					{
						continue;
					}

					DataInterface = FindCurveDataInterfaceFromNode(LinkedPin->GetOwningNode());
					if (DataInterface)
					{
						break;
					}
				}
			}

			if (!DataInterface)
			{
				continue;
			}

			TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DataInterface);
			if (!CurveObject.IsValid())
			{
				continue;
			}

			TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);

			const FString PinName = Pin->PinName.ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = PinName.FindLastChar(TEXT('.'), DotIndex)
				? PinName.Mid(DotIndex + 1)
				: PinName;
			CurveMap.Add(SimplifiedName, CurveObject);
		}

		TArray<UNiagaraDataInterface*> DeepInterfaces;
		CollectDataInterfacesFromObjectDeep(FunctionNode, DeepInterfaces);
		AddDebugField(InputsObject, TEXT("_DebugFunctionCallDIsCount"), FString::FromInt(DeepInterfaces.Num()));
		TArray<FString> DeepInterfaceNames;
		for (UNiagaraDataInterface* Interface : DeepInterfaces)
		{
			if (!Interface)
			{
				continue;
			}

			if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(Interface))
			{
				TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
				CurveMap.Add(Interface->GetName(), CurveObject);
				DeepInterfaceNames.Add(FString::Printf(TEXT("%s : %s"), *Interface->GetName(), *Interface->GetClass()->GetName()));
			}
		}
		AddDebugArrayField(InputsObject, TEXT("_DebugFunctionCallDIs"), DeepInterfaceNames);
		if (DeepInterfaceNames.Num() == 0)
		{
			AddDebugField(InputsObject, TEXT("_DebugFunctionCallDIs"), TEXT("<empty>"));
		}

		if (CurveMap.Num() == 0)
		{
			return false;
		}

		if (const TSharedPtr<FJsonObject>* ExactMatch = CurveMap.Find(InputName))
		{
			if (ExactMatch->IsValid())
			{
				InputsObject->SetObjectField(InputName, *ExactMatch);
				return true;
			}
		}

		for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : CurveMap)
		{
			if (Pair.Key.Contains(InputName, ESearchCase::IgnoreCase) || InputName.Contains(Pair.Key, ESearchCase::IgnoreCase))
			{
				InputsObject->SetObjectField(InputName, Pair.Value);
				return true;
			}
		}

		if (CurveMap.Num() == 1)
		{
			auto It = CurveMap.CreateConstIterator();
			if (It.Value().IsValid())
			{
				InputsObject->SetObjectField(InputName, It.Value());
				return true;
			}
		}

		TSharedPtr<FJsonObject> CurvesObject = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : CurveMap)
		{
			CurvesObject->SetObjectField(Pair.Key, Pair.Value);
		}

		TSharedPtr<FJsonObject> MultiCurveObject = MakeShared<FJsonObject>();
		MultiCurveObject->SetObjectField(TEXT("Curves"), CurvesObject);
		InputsObject->SetObjectField(InputName, MultiCurveObject);
		return true;
	}

	void CollectFunctionCallPinDebug(UNiagaraNodeFunctionCall* FunctionNode, TArray<FString>& OutEntries)
	{
		if (!FunctionNode)
		{
			return;
		}

		for (UEdGraphPin* Pin : FunctionNode->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			const FString Direction = (Pin->Direction == EGPD_Input) ? TEXT("In") : TEXT("Out");
			const FString DefaultObjName = Pin->DefaultObject ? Pin->DefaultObject->GetName() : TEXT("<null>");
			const FString DefaultObjClass = Pin->DefaultObject ? Pin->DefaultObject->GetClass()->GetName() : TEXT("<null>");
			OutEntries.Add(FString::Printf(TEXT("%s [%s] Default=%s (%s) Linked=%d"),
				*PinName, *Direction, *DefaultObjName, *DefaultObjClass, Pin->LinkedTo.Num()));
		}
	}

	void CollectDataInterfacesFromObjectDeep(const UObject* Owner, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!Owner)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
			{
				if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(ObjProp->GetObjectPropertyValue_InContainer(Owner)))
					{
						OutDataInterfaces.Add(DataInterface);
					}
				}
				continue;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* InnerPtr = StructProp->ContainerPtrToValuePtr<void>(Owner);
				CollectDataInterfacesFromStruct(InnerPtr, StructProp->Struct, OutDataInterfaces, 0);
				continue;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
				if (!InnerObjProp || !InnerObjProp->PropertyClass || !InnerObjProp->PropertyClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
				{
					continue;
				}

				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ObjPtr = Helper.GetRawPtr(Index);
					if (!ObjPtr)
					{
						continue;
					}

					if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(InnerObjProp->GetObjectPropertyValue(ObjPtr)))
					{
						OutDataInterfaces.Add(DataInterface);
					}
				}
			}
		}
	}

	void CollectCurveInterfacesFromOuter(const UObject* Outer, TArray<UNiagaraDataInterface*>& OutDataInterfaces)
	{
		if (!Outer)
		{
			return;
		}

		TArray<UObject*> ChildObjects;
		GetObjectsWithOuter(Outer, ChildObjects, true);

		for (UObject* Child : ChildObjects)
		{
			if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Child))
			{
				if (HasCurveDataInterface(DataInterface))
				{
					OutDataInterfaces.Add(DataInterface);
				}
			}
		}
	}

	UNiagaraDataInterface* SelectOuterCurveInterface(const TArray<UNiagaraDataInterface*>& Interfaces, const FString& ScriptPath, const FString& InputName)
	{
		UNiagaraDataInterface* BestInterface = nullptr;
		int32 BestScore = -1;
		const bool bHintedInput = IsCurveInputName(InputName);

		for (UNiagaraDataInterface* Interface : Interfaces)
		{
			if (!Interface)
			{
				continue;
			}

			if (!HasCurveDataInterface(Interface))
			{
				continue;
			}

			const FString Path = Interface->GetPathName();
			int32 Score = 0;

			if (!ScriptPath.IsEmpty() && Path.Contains(ScriptPath))
			{
				Score += 100;
			}

			if (!InputName.IsEmpty() && Path.Contains(InputName))
			{
				Score += 10;
			}

			if (!bHintedInput && !InputName.IsEmpty() && !Path.Contains(InputName))
			{
				continue;
			}

			if (Score > BestScore)
			{
				BestScore = Score;
				BestInterface = Interface;
			}
		}

		if (!BestInterface)
		{
			TArray<UNiagaraDataInterface*> CurveInterfaces;
			for (UNiagaraDataInterface* Interface : Interfaces)
			{
				if (!Interface)
				{
					continue;
				}

				if (HasCurveDataInterface(Interface))
				{
					CurveInterfaces.Add(Interface);
				}
			}

			if (CurveInterfaces.Num() == 1 && bHintedInput)
			{
				return CurveInterfaces[0];
			}
		}

		return BestInterface;
	}

	bool TryOverrideCurveScale(const FString& FunctionName, const FString& InputName, const TArray<FNiagaraVariable>& RapidIterationVariables,
		const FNiagaraParameterStore& RapidIterationParameters, const TSharedPtr<FJsonObject>& CurveObject)
	{
		if (!CurveObject.IsValid())
		{
			return false;
		}

		const FString ScaleToken = TEXT("Scale Curve");
		const FString CurveToken = TEXT("FloatFromCurve");
		const FNiagaraVariable* BestVariable = nullptr;
		int32 BestScore = -1;

		for (const FNiagaraVariable& Variable : RapidIterationVariables)
		{
			const FString VarName = Variable.GetName().ToString();
			if (!VarName.Contains(ScaleToken))
			{
				continue;
			}

			int32 Score = 0;
			if (VarName.Contains(CurveToken))
			{
				Score += 10;
			}
			if (!FunctionName.IsEmpty() && VarName.Contains(FunctionName))
			{
				Score += 5;
			}
			if (!InputName.IsEmpty() && VarName.Contains(InputName))
			{
				Score += 3;
			}

			if (Score > BestScore)
			{
				BestScore = Score;
				BestVariable = &Variable;
			}
		}

		if (!BestVariable)
		{
			return false;
		}

		if (RapidIterationParameters.IndexOf(*BestVariable) == INDEX_NONE)
		{
			return false;
		}

		const FNiagaraTypeDefinition ScaleType = BestVariable->GetType();
		if (ScaleType == FNiagaraTypeDefinition::GetFloatDef())
		{
			// Keep vector/color CurveScale authored on DI; avoid collapsing it to scalar.
			const TSharedPtr<FJsonObject>* ExistingScaleObj = nullptr;
			if (CurveObject->TryGetObjectField(TEXT("CurveScale"), ExistingScaleObj) && ExistingScaleObj && ExistingScaleObj->IsValid())
			{
				return false;
			}

			const float ScaleValue = RapidIterationParameters.GetParameterValue<float>(*BestVariable);
			CurveObject->SetNumberField(TEXT("CurveScale"), ScaleValue);
			return true;
		}

		if (ScaleType == FNiagaraTypeDefinition::GetVec2Def())
		{
			const FVector2f V = RapidIterationParameters.GetParameterValue<FVector2f>(*BestVariable);
			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("X"), V.X);
			ScaleObj->SetNumberField(TEXT("Y"), V.Y);
			CurveObject->SetObjectField(TEXT("CurveScale"), ScaleObj);
			return true;
		}

		if (ScaleType == FNiagaraTypeDefinition::GetVec3Def())
		{
			const FVector3f V = RapidIterationParameters.GetParameterValue<FVector3f>(*BestVariable);
			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("X"), V.X);
			ScaleObj->SetNumberField(TEXT("Y"), V.Y);
			ScaleObj->SetNumberField(TEXT("Z"), V.Z);
			CurveObject->SetObjectField(TEXT("CurveScale"), ScaleObj);
			return true;
		}

		if (ScaleType == FNiagaraTypeDefinition::GetVec4Def())
		{
			const FVector4f V = RapidIterationParameters.GetParameterValue<FVector4f>(*BestVariable);
			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("X"), V.X);
			ScaleObj->SetNumberField(TEXT("Y"), V.Y);
			ScaleObj->SetNumberField(TEXT("Z"), V.Z);
			ScaleObj->SetNumberField(TEXT("W"), V.W);
			CurveObject->SetObjectField(TEXT("CurveScale"), ScaleObj);
			return true;
		}

		if (ScaleType == FNiagaraTypeDefinition::GetColorDef())
		{
			const FLinearColor C = RapidIterationParameters.GetParameterValue<FLinearColor>(*BestVariable);
			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("R"), C.R);
			ScaleObj->SetNumberField(TEXT("G"), C.G);
			ScaleObj->SetNumberField(TEXT("B"), C.B);
			ScaleObj->SetNumberField(TEXT("A"), C.A);
			CurveObject->SetObjectField(TEXT("CurveScale"), ScaleObj);
			return true;
		}

		return false;
	}

	bool IsCurveInputName(const FString& InputName)
	{
		return InputName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase)
			|| InputName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase);
	}

	UEdGraphPin* FindParameterMapSetInputPinInGraph(UNiagaraGraph* Graph, const FString& InputName, const FString& FunctionName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapSet"))
			{
				continue;
			}

			UEdGraphPin* Pin = FindParameterMapSetInputPin(Node, InputName, FunctionName);
			if (Pin)
			{
				return Pin;
			}
		}

		return nullptr;
	}

	UNiagaraDataInterface* FindCurveDataInterfaceFromNode(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}

		if (const UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node))
		{
			if (FunctionNode->FunctionScript && FunctionNode->FunctionScript->GetUsage() == ENiagaraScriptUsage::DynamicInput)
			{
				TArray<UNiagaraDataInterface*> CurveInterfaces;
				CollectCurveDataInterfacesFromFunctionCall(const_cast<UNiagaraNodeFunctionCall*>(FunctionNode), CurveInterfaces);
				if (CurveInterfaces.Num() == 1)
				{
					return CurveInterfaces[0];
				}
			}
		}

		if (const UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
		{
			const FObjectProperty* DataInterfaceProperty = FindFProperty<FObjectProperty>(InputNode->GetClass(), TEXT("DataInterface"));
			if (DataInterfaceProperty)
			{
				if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(
						DataInterfaceProperty->GetObjectPropertyValue_InContainer(InputNode)))
				{
					return DataInterface;
				}
			}
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			if (UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Pin->DefaultObject))
			{
				return DataInterface;
			}

			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin)
				{
					continue;
				}

				if (UNiagaraNodeInput* LinkedInput = Cast<UNiagaraNodeInput>(LinkedPin->GetOwningNode()))
				{
					const FObjectProperty* DataInterfaceProperty = FindFProperty<FObjectProperty>(LinkedInput->GetClass(), TEXT("DataInterface"));
					if (DataInterfaceProperty)
					{
						if (UNiagaraDataInterface* LinkedDataInterface = Cast<UNiagaraDataInterface>(
								DataInterfaceProperty->GetObjectPropertyValue_InContainer(LinkedInput)))
						{
							return LinkedDataInterface;
						}
					}
				}
			}
		}

		return nullptr;
	}

	UEdGraphPin* FindFunctionInputPin(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName)
	{
		if (!FunctionNode)
		{
			return nullptr;
		}

		if (UEdGraphPin* ExactPin = FunctionNode->FindPin(InputName, EGPD_Input))
		{
			return ExactPin;
		}

		for (UEdGraphPin* Pin : FunctionNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			int32 DotIndex = INDEX_NONE;
			const FString SimplifiedName = PinName.FindLastChar(TEXT('.'), DotIndex)
				? PinName.Mid(DotIndex + 1)
				: PinName;

			if (SimplifiedName == InputName)
			{
				return Pin;
			}
		}

		return nullptr;
	}

	UEdGraphPin* FindParameterMapOutputPin(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
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

	void GatherParameterMapSetNodesFromOutput(UEdGraphPin* OutputMapPin, TArray<UEdGraphNode*>& OutSetNodes)
	{
		if (!OutputMapPin)
		{
			return;
		}

		TSet<UEdGraphNode*> VisitedNodes;
		TArray<UEdGraphPin*> PendingPins;
		PendingPins.Add(OutputMapPin);

		while (PendingPins.Num() > 0)
		{
			UEdGraphPin* CurrentPin = PendingPins.Pop(false);
			if (!CurrentPin)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : CurrentPin->LinkedTo)
			{
				if (!LinkedPin)
				{
					continue;
				}

				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
				if (!LinkedNode || VisitedNodes.Contains(LinkedNode))
				{
					continue;
				}

				VisitedNodes.Add(LinkedNode);

				if (LinkedNode->GetClass()->GetFName() == TEXT("NiagaraNodeParameterMapSet"))
				{
					OutSetNodes.Add(LinkedNode);
				}

				UEdGraphPin* NextOutputMap = FindParameterMapOutputPin(LinkedNode);
				if (NextOutputMap && NextOutputMap != CurrentPin)
				{
					PendingPins.Add(NextOutputMap);
				}
			}
		}
	}


	bool MatchesInputPinName(const FString& PinName, const FString& InputName, const FString& FunctionName)
	{
		const FString NormalizedInput = NormalizeInputToken(InputName);
		const FString NormalizedPin = NormalizeInputToken(PinName);
		FString LastToken = PinName;
		int32 LastDotIndex = INDEX_NONE;
		if (PinName.FindLastChar(TEXT('.'), LastDotIndex))
		{
			LastToken = PinName.Mid(LastDotIndex + 1);
		}
		const FString NormalizedLastToken = NormalizeInputToken(LastToken);
		if (!NormalizedInput.IsEmpty() && (NormalizedPin == NormalizedInput || NormalizedLastToken == NormalizedInput))
		{
			return true;
		}

		if (!InputName.IsEmpty() && PinName == InputName)
		{
			return true;
		}

		if (!InputName.IsEmpty() && PinName.Contains(TEXT(".") + InputName + TEXT(".")))
		{
			return true;
		}

		if (FunctionName.IsEmpty())
		{
			return !InputName.IsEmpty() && PinName.EndsWith(TEXT(".") + InputName);
		}

		const FString FunctionToken = TEXT(".") + FunctionName + TEXT(".");
		if (!PinName.Contains(FunctionToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			return !InputName.IsEmpty() && PinName.EndsWith(TEXT(".") + InputName);
		}

		if (InputName.IsEmpty())
		{
			return true;
		}

		const FString Suffix = TEXT(".") + InputName;
		return PinName.EndsWith(Suffix);
	}

	UEdGraphPin* FindParameterMapSetInputPin(UEdGraphNode* SetNode, const FString& InputName, const FString& FunctionName)
	{
		if (!SetNode)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : SetNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
			{
				continue;
			}

			if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
			{
				continue;
			}

			if (MatchesInputPinName(Pin->PinName.ToString(), InputName, FunctionName))
			{
				return Pin;
			}
		}

		return nullptr;
	}

	bool TrySetDynamicInputCurveValue(UNiagaraNodeFunctionCall* FunctionNode, const FString& InputName, const TSharedPtr<FJsonObject>& InputsObject,
		const FString& FunctionName, const TArray<FNiagaraVariable>& RapidIterationVariables, const FNiagaraParameterStore& RapidIterationParameters,
		const TArray<UNiagaraDataInterface*>& OuterCurveInterfaces, const FString& SourceScriptPath)
	{
		if (!FunctionNode || !InputsObject.IsValid())
		{
			return false;
		}

		TArray<FString> BoolPinDebug;

		UEdGraphPin* InputPin = FindFunctionInputPin(FunctionNode, InputName);
		AddDebugField(InputsObject, TEXT("_DebugInputPinFound"), InputPin ? TEXT("1") : TEXT("0"));
		if (InputPin)
		{
			AddDebugField(InputsObject, TEXT("_DebugInputPinName"), InputPin->PinName.ToString());
			AddDebugField(InputsObject, TEXT("_DebugInputPinLinks"), FString::FromInt(InputPin->LinkedTo.Num()));
		}

		auto TrySetFromLinkedPins = [&](const TArray<UEdGraphPin*>& LinkedPins)
		{
			for (UEdGraphPin* LinkedPin : LinkedPins)
			{
				if (!LinkedPin)
				{
					continue;
				}

				if (UNiagaraNodeFunctionCall* LinkedFunction = Cast<UNiagaraNodeFunctionCall>(LinkedPin->GetOwningNode()))
				{
					if (TrySetCurvesFromFunctionCallNode(LinkedFunction, InputName, InputsObject, FunctionName, RapidIterationVariables,
							RapidIterationParameters, OuterCurveInterfaces, SourceScriptPath))
					{
						return true;
					}
				}

				UNiagaraDataInterface* DataInterface = FindCurveDataInterfaceFromNode(LinkedPin->GetOwningNode());
				if (!DataInterface)
				{
					continue;
				}

				AddDebugField(InputsObject, TEXT("_DebugLinkedDIClass"), DataInterface->GetClass()->GetName());
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DataInterface))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}

			return false;
		};

		if (InputPin)
		{
			if (FunctionNode->FunctionScript && FunctionNode->FunctionScript->GetUsage() == ENiagaraScriptUsage::DynamicInput)
			{
				const FString DynamicInputName = FunctionNode->GetFunctionName();
				if (UNiagaraDataInterface* OuterCurve = SelectOuterCurveInterfaceForDynamicInput(OuterCurveInterfaces, SourceScriptPath, DynamicInputName, InputName))
				{
					if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(OuterCurve))
					{
						TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
						InputsObject->SetObjectField(InputName, CurveObject);
						return true;
					}
				}
			}

			if (InputPin->LinkedTo.Num() > 0 && TrySetFromLinkedPins(InputPin->LinkedTo))
			{
				return true;
			}

			if (TrySetBoolFromPin(InputPin, InputName, InputsObject))
			{
				return true;
			}

			if (UNiagaraDataInterface* DirectDataInterface = Cast<UNiagaraDataInterface>(InputPin->DefaultObject))
			{
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DirectDataInterface))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}
		}

		UEdGraphPin* OutputMapPin = FindParameterMapOutputPin(FunctionNode);
		if (!OutputMapPin)
		{
			if (UNiagaraGraph* OwnerGraph = Cast<UNiagaraGraph>(FunctionNode->GetGraph()))
			{
				if (UEdGraphPin* GraphSetPin = FindParameterMapSetInputPinInGraph(OwnerGraph, InputName, FunctionName))
				{
					if (GraphSetPin->LinkedTo.Num() > 0 && TrySetFromLinkedPins(GraphSetPin->LinkedTo))
					{
						return true;
					}

					if (TrySetBoolFromPin(GraphSetPin, InputName, InputsObject))
					{
						return true;
					}

					if (UNiagaraDataInterface* DirectDataInterface = Cast<UNiagaraDataInterface>(GraphSetPin->DefaultObject))
					{
						if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DirectDataInterface))
						{
							TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
							InputsObject->SetObjectField(InputName, CurveObject);
							return true;
						}
					}
				}

				TArray<UEdGraphNode*> GraphSetNodes;
				for (UEdGraphNode* Node : OwnerGraph->Nodes)
				{
					if (!Node || Node->GetClass()->GetFName() != TEXT("NiagaraNodeParameterMapSet"))
					{
						continue;
					}

					if (TrySetBoolFromPinNameMatch(Node, InputName, InputsObject))
					{
						return true;
					}
				}
			}

			return false;
		}

		TArray<UEdGraphNode*> SetNodes;
		GatherParameterMapSetNodesFromOutput(OutputMapPin, SetNodes);

		for (UEdGraphNode* SetNode : SetNodes)
		{
			if (!SetNode)
			{
				continue;
			}

			for (UEdGraphPin* Pin : SetNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetBoolDef().GetStruct())
				{
					BoolPinDebug.Add(Pin->PinName.ToString());
				}
			}

			for (UEdGraphPin* Pin : SetNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

			}

			UEdGraphPin* SetInputPin = FindParameterMapSetInputPin(SetNode, InputName, FunctionName);
			if (!SetInputPin)
			{
				if (TrySetBoolFromPinNameMatch(SetNode, InputName, InputsObject))
				{
					return true;
				}

				continue;
			}

			if (SetInputPin->LinkedTo.Num() > 0 && TrySetFromLinkedPins(SetInputPin->LinkedTo))
			{
				return true;
			}

			if (TrySetBoolFromPin(SetInputPin, InputName, InputsObject))
			{
				return true;
			}

			if (UNiagaraDataInterface* DirectDataInterface = Cast<UNiagaraDataInterface>(SetInputPin->DefaultObject))
			{
				if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DirectDataInterface))
				{
					TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
					InputsObject->SetObjectField(InputName, CurveObject);
					return true;
				}
			}
		}

		if (UNiagaraGraph* OwnerGraph = Cast<UNiagaraGraph>(FunctionNode->GetGraph()))
		{
			if (UEdGraphPin* GraphSetPin = FindParameterMapSetInputPinInGraph(OwnerGraph, InputName, FunctionName))
			{
				if (GraphSetPin->LinkedTo.Num() > 0 && TrySetFromLinkedPins(GraphSetPin->LinkedTo))
				{
					return true;
				}

				if (TrySetBoolFromPin(GraphSetPin, InputName, InputsObject))
				{
					return true;
				}

				if (UNiagaraDataInterface* DirectDataInterface = Cast<UNiagaraDataInterface>(GraphSetPin->DefaultObject))
				{
					if (TSharedPtr<FJsonObject> CurveObject = BuildCurveObjectFromDataInterface(DirectDataInterface))
					{
						TryOverrideCurveScale(FunctionName, InputName, RapidIterationVariables, RapidIterationParameters, CurveObject);
						InputsObject->SetObjectField(InputName, CurveObject);
						return true;
					}
				}
			}
		}

		AddDebugArrayField(InputsObject, TEXT("_DebugBoolPins"), BoolPinDebug);
		return false;
	}

	void GatherInputNamesFromFunctionNode(UNiagaraNodeFunctionCall* FunctionNode, TSet<FString>& OutInputNames)
	{
		if (!FunctionNode)
		{
			return;
		}

		const FString FunctionName = FunctionNode->GetFunctionName();
		UEdGraphPin* OutputMapPin = FindParameterMapOutputPin(FunctionNode);
		if (!OutputMapPin)
		{
			return;
		}

		TArray<UEdGraphNode*> SetNodes;
		GatherParameterMapSetNodesFromOutput(OutputMapPin, SetNodes);

		for (UEdGraphNode* SetNode : SetNodes)
		{
			if (!SetNode)
			{
				continue;
			}

			for (UEdGraphPin* Pin : SetNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin)
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				const FString PinName = Pin->PinName.ToString();
				if (!MatchesInputPinName(PinName, TEXT(""), FunctionName))
				{
					continue;
				}

				int32 DotIndex = INDEX_NONE;
				const FString InputName = PinName.FindLastChar(TEXT('.'), DotIndex)
					? PinName.Mid(DotIndex + 1)
					: PinName;

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
		const FString& UsageLabel, const TArray<UNiagaraDataInterface*>& FallbackDataInterfaces,
		TArray<TSharedPtr<FJsonValue>>& OutModules)
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

		TArray<UNiagaraDataInterface*> DynamicInputCurveInterfaces;
		CollectCurveDataInterfacesFromDynamicInputs(ScriptSource->NodeGraph, DynamicInputCurveInterfaces);

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

			if (InputNames.Num() == 0)
			{
				GatherInputNamesFromFunctionNode(FunctionNode, InputNames);
			}
			GatherInputNamesFromFunctionCallPins(FunctionNode, InputNames);
			{
				TArray<FString> InputNameList = InputNames.Array();
				InputNameList.Sort();
				AddDebugArrayField(InputsObject, TEXT("_DebugInputNames"), InputNameList);
			}

			TArray<FNiagaraVariable> RapidIterationVariables;
			SourceScript->RapidIterationParameters.GetParameters(RapidIterationVariables);

			const FString FunctionName = FunctionNode->GetFunctionName();
			TArray<UNiagaraDataInterface*> FunctionGraphCurveInterfaces;
			CollectCurveDataInterfacesFromFunctionCall(FunctionNode, FunctionGraphCurveInterfaces);
			if (FunctionGraphCurveInterfaces.Num() == 0 && DynamicInputCurveInterfaces.Num() > 0)
			{
				FunctionGraphCurveInterfaces = DynamicInputCurveInterfaces;
			}
			TArray<FString> FunctionCallPinDebug;
			CollectFunctionCallPinDebug(FunctionNode, FunctionCallPinDebug);
			AddDebugArrayField(InputsObject, TEXT("_DebugFunctionCallPins"), FunctionCallPinDebug);
			TArray<FString> FunctionGraphCurveNames;
			for (UNiagaraDataInterface* Interface : FunctionGraphCurveInterfaces)
			{
				if (Interface)
				{
					FunctionGraphCurveNames.Add(FString::Printf(TEXT("%s : %s"), *Interface->GetName(), *Interface->GetClass()->GetName()));
				}
			}
			AddDebugArrayField(InputsObject, TEXT("_DebugFunctionGraphCurves"), FunctionGraphCurveNames);
			TArray<FString> DynamicInputNodeDebug;
			CollectDynamicInputNodeDebug(FunctionNode, DynamicInputNodeDebug);
			AddDebugArrayField(InputsObject, TEXT("_DebugDynamicInputNodes"), DynamicInputNodeDebug);
			GatherInputNamesFromRapidIteration(FunctionName, RapidIterationVariables, InputNames);
			TArray<UNiagaraDataInterface*> OuterCurveInterfaces;
			UObject* OuterOwner = nullptr;
			if (FunctionNode->GetTypedOuter<UNiagaraSystem>())
			{
				OuterOwner = FunctionNode->GetTypedOuter<UNiagaraSystem>();
			}
			else if (FunctionNode->GetTypedOuter<UNiagaraEmitter>())
			{
				OuterOwner = FunctionNode->GetTypedOuter<UNiagaraEmitter>();
			}
			else
			{
				OuterOwner = ScriptSource;
			}
			CollectCurveInterfacesFromOuter(OuterOwner, OuterCurveInterfaces);
			TArray<FString> UnresolvedInputs;
			TArray<FString> UnresolvedInputTraces;
			for (const FString& InputName : InputNames)
			{
				bool bHandled = false;
				const bool bCurveHandled = TrySetDynamicInputCurveValue(FunctionNode, InputName, InputsObject, FunctionName, RapidIterationVariables,
						SourceScript->RapidIterationParameters, OuterCurveInterfaces, SourceScript->GetPathName());
				if (bCurveHandled)
				{
					bHandled = true;
					continue;
				}

				const bool bStaticSwitchHandled = TrySetStaticSwitchValue(FunctionNode, InputName, InputsObject);
				if (bStaticSwitchHandled)
				{
					bHandled = true;
					continue;
				}

				const bool bRapidHandled = TrySetRapidIterationValue(SourceScript, FunctionName, InputName, RapidIterationVariables, InputsObject,
						FallbackDataInterfaces, FunctionGraphCurveInterfaces, OuterCurveInterfaces);
				if (bRapidHandled)
				{
					bHandled = true;
					continue;
				}

				const bool bFunctionDefaultHandled = TrySetDefaultValueFromFunctionNode(FunctionNode, InputName, InputsObject);
				if (bFunctionDefaultHandled)
				{
					bHandled = true;
					continue;
				}

				const bool bModuleDefaultHandled = TrySetDefaultValueFromModuleScript(TargetModule, InputName, InputsObject);
				if (bModuleDefaultHandled)
				{
					bHandled = true;
					continue;
				}

				if (!bHandled)
				{
					UnresolvedInputs.Add(InputName);
					const FString Trace = FString::Printf(TEXT("%s: Curve=%d StaticSwitch=%d Rapid=%d FunctionDefault=%d ModuleDefault=%d"),
						*InputName,
						bCurveHandled ? 1 : 0,
						bStaticSwitchHandled ? 1 : 0,
						bRapidHandled ? 1 : 0,
						bFunctionDefaultHandled ? 1 : 0,
						bModuleDefaultHandled ? 1 : 0);
					UnresolvedInputTraces.Add(Trace);
				}
			}
			AddDebugArrayField(InputsObject, TEXT("_DebugUnresolvedInputs"), UnresolvedInputs);
			AddDebugArrayField(InputsObject, TEXT("_DebugUnresolvedInputTraces"), UnresolvedInputTraces);

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

					TArray<UNiagaraDataInterface*> FallbackDataInterfaces;
					if (EmitterData)
					{
						const UScriptStruct* EmitterStruct = FVersionedNiagaraEmitterData::StaticStruct();
						CollectDataInterfacesFromStruct(EmitterData, EmitterStruct, FallbackDataInterfaces, 0);
					}

					TArray<TSharedPtr<FJsonValue>> UsageModules;
					if (AppendModuleSnapshots(ModuleScript, Script, Usage, ScriptUsageToString(Usage), FallbackDataInterfaces, UsageModules))
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
namespace
{
	bool TryExtractVariableLikeName(const void* KeyPtr, const FProperty* KeyProp, FString& OutName)
	{
		OutName.Reset();
		if (!KeyPtr || !KeyProp)
		{
			return false;
		}

		if (const FNameProperty* NameProp = CastField<FNameProperty>(KeyProp))
		{
			OutName = NameProp->GetPropertyValue(KeyPtr).ToString();
			return !OutName.IsEmpty();
		}

		if (const FStrProperty* StrProp = CastField<FStrProperty>(KeyProp))
		{
			OutName = StrProp->GetPropertyValue(KeyPtr);
			return !OutName.IsEmpty();
		}

		const FStructProperty* KeyStructProp = CastField<FStructProperty>(KeyProp);
		if (!KeyStructProp || !KeyStructProp->Struct)
		{
			return false;
		}

		const UStruct* KeyStruct = KeyStructProp->Struct;
		if (KeyStruct->GetName().Contains(TEXT("NiagaraVariable"), ESearchCase::IgnoreCase))
		{
			const FNiagaraVariableBase* VarBase = reinterpret_cast<const FNiagaraVariableBase*>(KeyPtr);
			if (VarBase)
			{
				const FString Name = VarBase->GetName().ToString();
				if (!Name.IsEmpty())
				{
					OutName = Name;
					return true;
				}
			}
		}

		if (const FNameProperty* NameField = FindFProperty<FNameProperty>(KeyStruct, TEXT("Name")))
		{
			OutName = NameField->GetPropertyValue_InContainer(KeyPtr).ToString();
			return !OutName.IsEmpty();
		}

		if (const FStrProperty* StrField = FindFProperty<FStrProperty>(KeyStruct, TEXT("Name")))
		{
			OutName = StrField->GetPropertyValue_InContainer(KeyPtr);
			return !OutName.IsEmpty();
		}

		// Generic fallback: scan any FName/FString field in the key struct.
		for (TFieldIterator<FProperty> It(KeyStruct); It; ++It)
		{
			const FProperty* Field = *It;
			if (!Field)
			{
				continue;
			}

			if (const FNameProperty* AnyName = CastField<FNameProperty>(Field))
			{
				const FString Value = AnyName->GetPropertyValue_InContainer(KeyPtr).ToString();
				if (!Value.IsEmpty())
				{
					OutName = Value;
					return true;
				}
			}
			else if (const FStrProperty* AnyStr = CastField<FStrProperty>(Field))
			{
				const FString Value = AnyStr->GetPropertyValue_InContainer(KeyPtr);
				if (!Value.IsEmpty())
				{
					OutName = Value;
					return true;
				}
			}
		}

		return false;
	}
}
namespace
{
	void AddDebugModuleGraphValuePins(UNiagaraGraph* Graph, const TSharedPtr<FJsonObject>& InputsObject)
	{
		if (!Graph || !InputsObject.IsValid())
		{
			return;
		}

		TArray<FString> PinEntries;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			const FString NodeClass = Node->GetClass()->GetName();
			if (!NodeClass.Contains(TEXT("ParameterMapGet")) && !NodeClass.Contains(TEXT("ParameterMapSet")))
			{
				continue;
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bOrphanedPin)
				{
					continue;
				}

				if (Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
				{
					continue;
				}

				const FString Dir = (Pin->Direction == EGPD_Input) ? TEXT("In") : TEXT("Out");
				const FString TypeName = Pin->PinType.PinSubCategoryObject.IsValid()
					? Pin->PinType.PinSubCategoryObject->GetName()
					: Pin->PinType.PinCategory.ToString();
				const FString Def = GetPinDefaultString(Pin);
				PinEntries.Add(FString::Printf(TEXT("%s %s %s Type=%s Default=%s"),
					*NodeClass, *Dir, *Pin->PinName.ToString(), *TypeName, *Def));
			}
		}

		AddDebugArrayField(InputsObject, TEXT("_DebugModuleGraphValuePins"), PinEntries);
	}
}
