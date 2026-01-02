---
marp: true
theme: default
class: invert
#backgroundImage: 
---
# Material Layerを使ったVFX汎用マテリアル
---
# アジェンダ
- ## Material Layerとは
- ## 作ったもの紹介
- ## 制作背景
- ## マテリアルの構造
- ## メリット、デメリット
---

# Material Layersとは
- 複数の「表面（レイヤー）」を積んで、マスクでブレンドして1つのMaterialとして出力する仕組み
- 目的：複雑な見た目（例：下地金属＋塗装＋汚れ＋傷）を 利用/調整しやすい
- Material Instance Editor 上でレイヤーを差し替えができる
- 似たような名前のLayered Materialとは別機能 (紛らわしい)
  - https://dev.epicgames.com/documentation/ja-jp/unreal-engine/layered-materials-in-unreal-engine
---
## 動画
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---

## 構成要素（2つのアセット）
- Material Layer Asset
  - 1レイヤー分のマテリアルロジック（テクスチャ/パラメータ含む）を持つ
- Material Layer Blend Asset
  - 下レイヤー/上レイヤーの混ぜ方（マスクやLerp）を定義する
- どちらも 専用のグラフを持ち、作成したものを使い回しできる

---

# 作ったもの紹介
---
## テストレベル
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---
## エディタ動画
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---
## 実装した機能
### UV アニメーション
- Scaling,Scroll, Rotation, Shear
- Radial UV
- Random Offset
- Dynamic Parameter Scale Offset
- Distortion
- UV FlipBook

---
### マテリアルレイヤー
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
### マテリアルブレンド
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
# マテリアルの構造
---
# メリット、デメリット
---
## メリット
---
### 機能追加が圧倒的に楽になった
- 追加にかかる時間が圧倒的に減った
  - 機能によっては数日→数時間レベルに
- アセットが独立しているためユニットテストが容易
  - より自信をもって機能リリースできるように
---
### BaseMaterialがシンプルに
- ノード数が圧倒的に減った
  - StaticSwitchを使った分岐制御から解放された
- コンパイル時間が減った(気がする)
- ノード数が多いとコンパイルが失敗するUEのバグが発生しなくなった(当時)
---
### バグが少なくなった
- 機能修正がレイヤー/ブレンドアセット単位になることが多い
  - 局所的な修正を設計レベルで保障できる
- マテリアル全体が壊れにくくなった
---
## デメリット
---
### 無限に重いマテリアルがアーティスト自身で作成できてしまう
- その気になればテクスチャを大量にサンプリングするマテリアルや、非常に重い処理を行う機能を重ねることができてしまう
- 運用していたプロジェクトでレギュレーション違反するマテリアルが目立つようになった
- 実際に運用する際はレギュレーション監視ツールが必要
---
### マテリアルのバリエーションは増加しやすくなる
- MIで自由に処理を定義できる都合上、バリエーション自体は増加しがち
- 粒子などシンプルな構成のマテリアルはこの機能を使用しないかテンプレートを切ったほうが良い
- UDNでマテリアルレイヤー特有のバリエーション増加があるか質問している人がいるがEpicが回答せず...
---
### 同一マテリアル内の処理の使いまわしには注意が必要
- レイヤー間でテクスチャのサンプル結果や特定の計算(UV計算など)をそのまま使いまわすことはできない
  - やり方はあるが少しハック的な手法が必要
---
### Attribute Bindingが使用できない(Niagara)
- BaseMaterial側のパラメータしかBindingが使用できない
