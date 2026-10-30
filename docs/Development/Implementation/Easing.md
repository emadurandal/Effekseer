# Easing Implementation

Effekseer の easing は、正規化された生存時間 `time` を補間率 `t` に変換し、その `t` で開始値、終了値、必要なら中間値を補間する。
この文書はユーザー向け機能説明ではなく、editor のデータ、バイナリ、runtime、shader の対応を追うための内部実装メモである。

## Source Map

| Layer | Main files | Role |
| --- | --- | --- |
| Editor data | `Dev/Editor/EffekseerCore/Data/Define.cs` | `EasingType`, `EasingStart`, `EasingEnd`, `FloatEasingParamater`, `Vector3DEasingParamater` などの UI/保存対象データ |
| Editor binary export | `Dev/Editor/EffekseerCore/Binary/Define.cs` | 共通の float/vector3 easing を runtime 用バイナリへ出力 |
| Runtime common easing | `Dev/Cpp/Effekseer/Effekseer/Parameter/Effekseer.Easing.h`, `.cpp` | `ParameterEasing<T>` による float/vector3 easing の読み込み、初期化、評価 |
| Legacy easing structs | `Dev/Cpp/Effekseer/Effekseer/Effekseer.InternalStruct.h` | `easing_float`, `easing_vector2d`, `easing_vector3d`, `easing_color` などの旧形式 |
| Renderer-specific export | `Dev/Editor/EffekseerCore/Binary/RendererValues.cs` | 色、リング位置など、旧形式 easing を使う renderer 固有データ |
| GPU particles | `Dev/Editor/EffekseerCore/Binary/GpuParticlesValues.cs`, `Dev/Cpp/Effekseer/Effekseer/Parameter/Effekseer.GpuParticlesParameter.cpp`, renderer shader | GPU particle の scale/color easing。3 次式係数だけを shader に渡す |

## Easing Type ABI

Editor の `EasingType` と runtime の `Easing3Type` は同じ整数値を使う。
これはプロジェクト/バイナリ互換性に関わるため、既存値を変更してはいけない。

| Editor | Runtime | Value | Meaning |
| --- | --- | ---: | --- |
| `LeftRightSpeed` | `StartEndSpeed` | 0 | Start/End speed から 3 次式係数を生成する旧来の easing |
| `Linear` | `Linear` | 1 | `t` をそのまま使う |
| `EaseInQuadratic` / `EaseOutQuadratic` / `EaseInOutQuadratic` | same | 10-12 | 2 次 |
| `EaseInCubic` / `EaseOutCubic` / `EaseInOutCubic` | same | 20-22 | 3 次 |
| `EaseInQuartic` / `EaseOutQuartic` / `EaseInOutQuartic` | same | 30-32 | 4 次 |
| `EaseInQuintic` / `EaseOutQuintic` / `EaseInOutQuintic` | same | 40-42 | 5 次 |
| `EaseInBack` / `EaseOutBack` / `EaseInOutBack` | same | 50-52 | overshoot する Back easing。runtime の係数は `1.8` |
| `EaseInBounce` / `EaseOutBounce` / `EaseInOutBounce` | same | 60-62 | piecewise な Bounce easing |

`StartEndSpeed` 以外は runtime 側の `ParameterEasing<T>::getEaseValue` が式を直接評価する。
`StartEndSpeed` だけは editor の export 時に 3 つの係数へ変換され、runtime と shader はその係数を評価する。

## Start/End Speed Cubic

古い `Memo/easing3.py` に置かれていた内容は、この Start/End speed 形式の係数計算である。
現在の実装では `Dev/Editor/EffekseerCore/Utils/Math.cs` の `MathUtl.Easing` が同じ式を持つ。

前提は、開始点を `(0, 0)`、終了点を `(1, 1)` とする 3 次式である。

```text
y = a * t^3 + b * t^2 + c * t
```

UI の Start/End speed は角度として保存される。
角度 `0` は傾き 1、つまり 45 度の直線を表す。

```text
g1 = tan((startAngle + 45) * pi / 180)
g2 = tan((endAngle   + 45) * pi / 180)

c = g1
a = g2 - g1 - 2 * (1 - c)
b = (g2 - g1 - 3 * a) / 2
```

この係数は `y(0) = 0`, `y(1) = 1`, `y'(0) = g1`, `y'(1) = g2` を満たす。
Start/End speed enum は `-30, -20, -10, 0, 10, 20, 30` を使い、`0, 0` なら線形になる。

評価式は runtime の `getEasingStartEndValue`、旧 `easing_*` 構造体、GPU particle shader の `EasingSpeed` で同じ形になっている。

```text
E(t) = a * t^3 + b * t^2 + c * t
```

## Runtime ParameterEasing

`ParameterEasing<T>` は `float` と `SIMD::Vec3f` に特殊化されている。
`InstanceEasing<T>` はインスタンスごとに決まる `start`, `middle`, `end`, `Rate` を保持する。

初期化時の流れは次の通り。

1. `Load` がバイナリから開始/終了の乱数範囲、dynamic equation 参照、中間点、easing type、ランダムグループ、軸別 type を読む。
2. `Init` が dynamic equation を適用し、開始/終了/中間の乱数をインスタンスごとに確定する。
3. 中間点が有効な場合、`Rate` は `start -> middle` と `middle -> end` の距離比から求める。float は絶対値、vector3 はベクトル長を使う。
4. 更新時に `GetValue(instance, time)` が `time` を easing type で `t` に変換し、2 点または 3 点補間を実行する。

2 点補間は単純な線形補間である。

```text
value = start + (end - start) * t
```

3 点補間は `start`, `middle`, `end` の自然 cubic spline である。
`Rate` を境界として `t` を `[0, 2]` の 2 区間に再配置し、前半を `start -> middle`、後半を `middle -> end` として評価する。
隣接点が同値の場合は、その区間で同じ値を返して不要な揺れを避ける。

軸別 type が有効な vector3 easing では、X/Y/Z ごとに別の easing type で `t` を計算する。
実装上は各軸の type で vector3 全体を評価したあと、X は X 用評価結果の X、Y は Y 用評価結果の Y、Z は Z 用評価結果の Z を合成する。

ランダムグループは vector3 の各軸が乱数を共有するための仕組みである。
export では X/Y/Z のグループ ID を 1 byte ずつ pack し、runtime は同じ ID の軸に同じ乱数を使う。
未対応だった古いバージョンでは X/Y/Z がそれぞれ独立した channel として扱われる。

## Binary Layout

共通の `FloatEasingParamater` と `Vector3DEasingParamater` は、概ね次の順で出力される。

1. 開始/終了の dynamic equation 参照。
2. 開始/終了の random value。
3. 中間点の有効フラグ。中間点が有効なら中間点の dynamic equation 参照と random value。
4. `EasingType`。
5. `EasingType.LeftRightSpeed` の場合のみ、Start/End speed から求めた 3 係数。
6. ランダムグループ channel。
7. 軸別 type 有効フラグ。有効なら各要素の `EasingType`。

`ParameterEasing<T>::Load` は、`minAppendParameterVersion_` より古いデータでは easing type や中間点を読まず、旧来の 3 係数だけを読む。
float easing の一部は `Version16Alpha9` 以降でサイズ prefix を持つため、`LoadFloatEasing` が旧サイズ固定形式と新形式を切り替えている。

単位変換は export 側で行う。
たとえば rotation easing は度からラジアンへ、scale/location は magnification を掛けた値として出力される。

## Legacy Easing

すべての easing が `ParameterEasing<T>` に乗っているわけではない。
renderer 固有の色、リングの 2D 位置、fade in/out などは、旧構造体を使う経路が残っている。

| Struct | Main use | Features |
| --- | --- | --- |
| `easing_float_without_random` | fade in/out など | Start/End speed 由来の 3 係数だけ。乱数なし |
| `easing_float` | 古い float easing 互換 | 開始/終了 random と 3 係数 |
| `easing_vector2d` / `easing_vector3d` | renderer 固有の位置など | 開始/終了 random と 3 係数 |
| `easing_color` | standard color / ribbon / ring / track など | 開始/終了 color random と 3 係数。HSVA の場合は補間後に RGB へ変換 |

これらの旧形式は named easing、middle point、random group、axis individual type を持たない。
そのため新しい easing type を追加しても、旧形式を使う renderer 固有パラメータには自動では反映されない。

## GPU Particles

GPU particle の easing はさらに軽量で、scale と color の Start/End speed だけを扱う。
editor は `GpuParticlesValues.cs` で `MathUtl.Easing` を呼び、3 係数を `float3` として出力する。
runtime は `LoadGpuParticlesParameter` で `Scale.Easing.Speed` と `RenderColor.ColorAll.Easing.Speed` に読み込み、renderer は uniform の `ScaleEasing` / `ColorEasing` として shader に渡す。

shader 側は次の式だけを評価し、`lerp` / `mix` の係数に使う。

```text
EasingSpeed(t, params) = params.x * t^3 + params.y * t^2 + params.z * t
```

GPU particle easing には、named easing、middle point、random group、axis individual type はない。
GPU particle に named easing を追加する場合は、editor のデータ、binary、`ParamSet`、uniform、各 renderer の shader をまとめて拡張する必要がある。

## Change Checklist

easing type を追加または変更する場合は、少なくとも次を確認する。

1. `Data.EasingType` と `Easing3Type` の整数値を一致させる。既存値は変更しない。
2. runtime の `getEaseValue` に評価式を追加する。
3. UI 表示用の language key を追加する。
4. `LeftRightSpeed` の式を変更する場合は、`MathUtl.Easing`、runtime の 3 係数評価、旧 `easing_*`、GPU particle shader の評価式が同じ前提になっているか確認する。
5. renderer 固有の旧形式や GPU particle にも同じ機能が必要か確認する。共通 easing を変更しても、それらには自動で反映されない。
