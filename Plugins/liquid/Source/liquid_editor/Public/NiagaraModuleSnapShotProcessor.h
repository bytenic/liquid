#pragma once

#include "CoreMinimal.h"

class UNiagaraScript;

/**
 * @brief Niagara モジュール入力のスナップショット出力クラス。
 *
 * 指定パス配下の Niagara System を走査し、指定した Niagara Module Script の
 * Emitter ごとの入力値を JSON として出力する。
 */
class FNiagaraModuleSnapshotProcessor
{
public:
	/** @brief Script の Usage が `Module` のとき true を返す。 */
	static bool IsModuleScript(const UNiagaraScript* Script);
	/** @brief `/Game` 形式のパスをトリムして正規化する。 */
	static FString NormalizeGamePath(const FString& Path);
	/** @brief `/Game` 配下の有効な LongPackageName かを検証する。 */
	static bool IsValidGamePath(const FString& Path);
	/**
	 * @brief Niagara System 内で一致したモジュール使用箇所のスナップショット JSON を生成する。
	 * @param ModuleScript 対象の Niagara Module Script アセット。
	 * @param SearchPath 再帰検索を行う `/Game` ルートパス。
	 * @param OutErrorMessage false を返した場合のエラーメッセージ。
	 * @param OutOutputPath 生成された JSON の絶対出力パス。
	 * @return 成功時 true。
	 */
	static bool CreateSnapshot(UNiagaraScript* ModuleScript, const FString& SearchPath, FString& OutErrorMessage, FString& OutOutputPath);
};
