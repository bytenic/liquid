---
marp: true
theme: default
class: invert
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
---
## 動画
<div style="text-align:center;">
  <video src="img/material_layer_doc.mp4" controls style="width:60%; height:auto;"></video>
</div>

---

# 構成要素（2つのアセット）
- Material Layer Asset
  - 1レイヤー分のマテリアルロジック（テクスチャ/パラメータ含む）を持つ
- Material Layer Blend Asset
  - 下レイヤー/上レイヤーの混ぜ方（マスクやLerp）を定義する
- どちらも 専用のグラフを持ち、作成したものを使い回しできる

---

# 作ったもの紹介

---
# 実装機能
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
