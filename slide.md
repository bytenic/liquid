---
marp: true
theme: default
class: invert
backgroundImage: 
size: 16:9
paginate: true
style: |
  section {
    background-image:
      linear-gradient(rgba(0,0,0,.45), rgba(0,0,0,.45)),
      url('img/background.jpg');
    background-size: cover;
    background-position: center;
  }

  section:not(.keep-size) ul,
  section:not(.keep-size) ol,
  section:not(.keep-size) li {
    font-size: 20pt;
  }
---
# Material Layerを使ったVFX汎用マテリアル
---
# アジェンダ
- ## Material Layerとは
- ## 作ったもの紹介
- ## 制作背景
- ## マテリアルの構造
- ## メリット, デメリット, 注意箇所
---
# Material Layersとは
- 複数の「表面（レイヤー）」を積んで、マスクでブレンドして1つのMaterialとして出力する仕組み
- 複雑な見た目（例：下地金属＋塗装＋汚れ＋傷）を 利用/調整しやすい
- Material Instance Editor 上でレイヤーの差し替えができる
- 似たような名前のLayered Materialとは別機能 (紛らわしい)
  - https://dev.epicgames.com/documentation/ja-jp/unreal-engine/layered-materials-in-unreal-engine
---
# 動画
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---
# 作ったもの紹介
---
# テストレベル
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---
# エディタ動画
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---
# 実装した機能
## UV アニメーション
- Scaling,Scroll, Rotation, Shear
- Radial UV
- Random Offset
- Dynamic Parameter Scale Offset
- Distortion
- UV FlipBook
---
## マテリアルレイヤー
- Texture Sampling(sRGB, Alpha, Normal)
- Procedural Mask
    - Circle
    - Triangle
    - Quad
    - UV
    - Super Ellipse
- Scene Color Distortion
- 6 Point Light
---
## マテリアルブレンド
<!-- _class: invert two-col compact -->
<style>
section.two-col ul { columns: 2; column-gap: 1; }
</style>
- Mask
- Dissolve
- Edge Color
- Directional Dissolve
- Color Gradient(LUT)
- Alpha Gradient(LUT)
- Gradient Color Dissolve
- Texture Blend(Additive, Multiply,Subtract)
- Depth Fade
- Scaling Sprite
- Dither Alpha
- Rim Fade
- Rim Gradient(LUT)
- Fake Point Light(Ignore Collision)
- Emissive Normal
---
# 制作背景
---
# 元々はUber Materialで運用
- 機能別にStaticSwitchで分岐
<image />

---
# プロジェクトが進むにつれてマテリアルが巨大化
- 前任者から引き継いだ時点でかなりの機能数があった
- 表現を模索している段階もあり追加要望が大量にあった
---
# 最初はそれなりに順調だったが...
- 機能拡張が進むにつれ実装が大変になってきた
- 単純な機能拡張は問題ないが全体の制御フローにかかわる拡張が大変
  - マスクテクスチャの値を複数の箇所で使いたい
  - LUTの適用箇所をマテリアルによって変えたい etc..

---
# 機能はまだ追加する必要があるがどうにかシンプルにできないか
- ノードグラフの整理やMaterialFunctionの切り分けは根本的な解決にならない
  - StaticSwitchによる分岐制御構造自体に問題がある
  - Materialの切り分けも管理するマテリアルが増えるリスクがある
## → MaterialLayerを使うのが良さそう
---
# マテリアルの構造

---
# ノード全体
<image />

---
# 処理は大きく3つのフェーズに分かれる
1. 共通処理
2. レイヤースタック
3. ポスト処理

<image />

---
# Base Material共通処理
- 全レイヤーが共通して使用できる値の計算を行う
  - 基本UV
  - Distortion(歪み)用UV
  - Dynamic Parameter(Niagaraから設定できる頂点Attribute)

---

# レイヤースタック評価
- アーティスト側がMaterialInstance(MI)で設定する処理を実行する
- マテリアルグラフ上ではこのノード一つで表現される
<image グラフ/>
<image MI/>

---
# レイヤースタック処理構造
- Base Materialの処理を入力として下から上へと処理される
<image グラフ/>
---
# レイヤースタックの処理
<image レイヤースタックのハイライト/>

---
# 各レイヤーの処理準は変更可能
- Bottom Layerと呼ばれる特殊なレイヤーを除いて実行順は変更できる
<image />
<movie />

---
# 各レイヤースタック間の値の受け渡し
- MaterialAttributeノードを下位スタックの出力→上位スタックの入力として受け取る
  - Unlitの場合はEmissiveColor(float3)とOpacity(float)のみなので非常にシンプル
<image />

---
# HLSLの疑似コード
##  MaterialAttribute定義
```
struct MaterialAttribute 
{
  float3 BaseColor;
  
}
```
---
## レイヤースタック処理関数疑似コード
```
MaterialAttribute EvaluateLayerStack(MaterialAttribute BaseMaterialResult)
{
  MaterialAttribute BottomLayerResult = EvaluateBottomLayer(BaseMaterialResult);
  
  MaterialAttribute Layer0Result = EvaluateLayer0(BottomLayerResult);
  MaterialAttribute Layer1Result = EvaluateLayer1(Layer0Result);
  MaterialAttribute Layer2Result = EvaluateLayer2(Layer1Result);
  MaterialAttribute Layer3Result = EvaluateLayer3(Layer2Result);

  return Layer3Result;
}

```
--- 
# レイヤーの処理
## Material Layer
  - このレイヤーで使用したい値を用意する
    - 例:マスク処理→マスクテクスチャのサンプリングしてMaterialAttributeとして出力
## Material Layer Blend
- 下レイヤーの結果のMaterialAttributeとMaterialLayerが出力したMaterialAttributeを入力として上位レイヤーへの出力となるMaterialAttributeを作成
  - 例:マスク処理→Material Layerのテクスチャサンプル結果をOpacityに乗算して出力
---
## レイヤー処理関数疑似コード
```
MaterialAttribute EvaluateLayer(MaterialAttribute BottomLayer)
{
  MaterialAttribute LayerResult = EvaluateLayer();
  MaterialAttribute LayerBlendResult = EvaluateLayerBlend(BottomLayer, LayerResult);

  return LayerBlendResult;
}
```
---
# 余計な処理が多そう...
- 実際にはマテリアルグラフからHLSLへの変換時に不要な変数や処理は削除された状態で展開される
  - 上記の疑似コードのような構造体や関数は使用しない
- レイヤースタックを使用することによる固有のオーバーヘッドは発生しない
```
//ここに出力HLSLのコード

```
---
# ただし例外があるかも
- 使用するノードによっては処理負荷が上がりそうなものがあるかも(最近知った)
  - BlendMaterialAttributesノードはかなり怪しい(このシステムでは未使用)
- 運用していた実装範囲ではUberMaterialと比べて処理負荷が増加するようなものは見受けられなかった
- あくまでEditorのStat上の命令数での観測なので正確な計測は各プラットフォームのプロファイラを使用したほうが良い

---
# メリット、デメリット
---
# メリット
---
# 機能追加が圧倒的に楽になった
- 追加にかかる時間が圧倒的に減った
  - 機能によっては数日→数時間レベルに
- アセットが独立しているためユニットテストが容易
  - より自信をもって機能リリースできるように
---
# BaseMaterialがシンプルに
- ノード数が圧倒的に減った
  - StaticSwitchを使った分岐制御から解放された
- コンパイル時間が減った(気がする)
- ノード数が多いとコンパイルが失敗するUEのバグが発生しなくなった(当時)
---
# バグが少なくなった
- 機能修正がレイヤー/ブレンドアセット単位になることが多い
  - 局所的な修正を設計レベルで保障できる
- マテリアル全体が壊れにくくなった
---
# デメリット、注意箇所
---
# MIで思いマテリアルが作成できてしまう
- その気になればテクスチャを大量にサンプリングするマテリアルや、非常に重い処理を行う機能を重ねることができる
  - MI(Material Instance)で作成できる = アーティスト環境で実現できてしまう
- 運用していたプロジェクトでレギュレーション違反するマテリアルが目立つようになった
- 実際に運用する際はレギュレーション監視ツールが必要
---
# マテリアルのバリエーションは増加しやすい
- MIで自由に処理を定義できる都合上、バリエーション自体は増加しがち
- 粒子などシンプルな構成のマテリアルはこの機能を使用しないかテンプレートを切ったほうが良い
- UDNでマテリアルレイヤー特有のバリエーション増加があるか質問している人がいるがEpicが回答せず...
---
# 同一マテリアル内の処理の使いまわしには注意が必要
- レイヤー間でテクスチャのサンプル結果や特定の計算(UV計算など)をそのまま使いまわすことはできない
  - やり方はあるが少しハック的な手法が必要
---
# Attribute Bindingが使用できない(Niagara)
- BaseMaterial側のパラメータしかBindingが使用できない
