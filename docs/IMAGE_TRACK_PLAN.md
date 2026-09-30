# Image Track 計画書（2026-09-30）

## 決定済み（ユーザー回答）

- 形態: 字幕とは別の **Image Track** にする（1 セクション＝1 枚）。
- 「下から上にフェードイン」: **両方作る**。
  - 滑り上がり＋フェード
  - 下からのワイプ（境目ぼかし）
- 位置の変更: **両方**。
  - Phase 1: 開始位置・終了位置のプリセット＋ビューポートでのドラッグ
  - Phase 2: キーフレーム
- 素材: **テクスチャ（Texture2D）のみ**。
- 入れる場所: **Sequencer Subtitles に同梱**。
- 退場のタイミング: **セクション内で完了**（セクションの終わりの Duration 秒で退場する）。

## 前提の調査

- 単独で画像を表示する機能は、今のコードにはない。画像まわりは、メッセージウィンドウの背景画像と区切り線の画像だけ。
- CinematicADV と manga-symbols-ue5 にも、該当する機能はなかった。
- UE の標準機能で近いのは、UMG の Widget Animation を Event Track から再生する方法。
  - ただし、Sequencer 上で直接置いてプレビューしたり、スクラブしたりはできない。
  - そのため、字幕と同じ方式で専用トラックを作る意味はある。

## 設計方針

1. **エフェクトの進み具合は、シーケンスの時間から計算する**（字幕のような Slate タイマーは使わない）。
   - スクラブで前後に動かせる。逆再生でも正しく見える。
   - Movie Render Queue での書き出しでも、フレーム単位で正確になる。
   - CinematicADV のスキップにもそのまま追従する。
2. **表示は、字幕と同じオーバーレイ（同じ DPI 処理）に画像用のレイヤーを追加する。**
   - 字幕の前に出すか後ろに出すかは、セクションごとに選べる。
3. **エフェクトは C++ だけで描く**（マテリアルアセットは不要）。
   - 自前の Slate ウィジェット `SSequencerImage` の中で、UV 範囲と頂点カラーのグラデーションを使って描く。
   - ワイプの境目ぼかし、分割、アイリスなども、アセットなしで実現する。
4. **イージングは自前の enum を用意する。**
   - UE 標準の `EEasingFunc` には Back（行き過ぎて戻る）や Bounce がないため。

## Phase 1（最初の実装）

### トラックとセクション
- `UMovieSceneSeqImageTrack`（複数の行に置ける）
- `UMovieSceneSeqImageSection`
- `FSequencerImageEvalTemplate`
- エディタ: 「+ Track」メニューに **Image Track** を追加。「+」ボタンでセクションを追加。

### レイアウト
| 項目 | 内容 |
|---|---|
| Anchor | 9 か所（左上〜右下） |
| Offset | px（ビューポートでドラッグして設定できる） |
| Size | 原寸 / 固定 (W,H) / 画面幅の % / 画面高さの % / 画面に収める / 画面を覆う |
| 見た目 | Tint、Opacity、左右反転、上下反転 |
| Layer | 字幕の後ろ（既定）/ 字幕の前 |

### 登場・退場エフェクト（それぞれ Duration と Easing を指定）
| エフェクト | パラメータ |
|---|---|
| Fade | — |
| Slide | 方向（4 方向）、距離（画面外から / 指定 px）、同時にフェードするか |
| Wipe | 方向（4 方向）、境目のぼかし幅 |
| Split（割れて合体） | 左右 / 上下。登場は外から合体、退場は割れて去る |
| Barn Door | 中央から外へ開くワイプ（左右 / 上下） |
| Zoom | 開始倍率 |
| Pop | 弾むズーム |
| Rotate | 回転角＋ズーム |
| Flip | カードをめくるような反転（横 / 縦） |
| Iris | 円形に開く / 閉じる。境目のぼかし付き |
| Blinds | 短冊の本数、方向、順番に出るときのずらし |

- **Easing:** Linear / EaseIn / EaseOut / EaseInOut / Back / Bounce / Elastic
- **退場の範囲:** セクション終わりの Duration 秒の中で完了させる（下の Q2 を参照）

### 表示中の効果
- None
- Tremble（字幕と同じ揺れ）
- Float（上下にふわふわ）
- Pulse（拡大縮小を繰り返す）
- Ken Burns（ゆっくりズーム＋パン）

### 位置の移動（プリセット）
- 開始 Offset → 終了 Offset を、指定した時間（またはセクション全体）で移動する。Easing 付き。

## Phase 1 の実装状況（コミット済み・**未ビルド**）

| ファイル | 内容 |
|---|---|
| `Public/SeqImageTypes.h` | 列挙型と設定の構造体（Layout / Transition / Continuous / Motion / Params） |
| `Public/SeqImageSection.h`, `Private/SeqImageSection.cpp` | `UMovieSceneSeqImageSection` |
| `Public/SeqImageTrack.h`, `Private/SeqImageTrack.cpp` | `UMovieSceneSeqImageTrack` |
| `Public/SeqImageEvalTemplate.h`, `Private/SeqImageEvalTemplate.cpp` | 時間駆動の評価。区間外なら削除 |
| `Private/SSeqImage.h/.cpp` | マスク系エフェクト（Wipe / Barn Door / Iris / Blinds / Split）の描画 |
| `Private/SubtitleSubsystemImages.cpp` | 画像レイヤーの管理、エフェクト・イージング・移動の計算、ドラッグでの位置調整 |
| Editor: `SeqImageTrackEditor.h/.cpp` | 「+ Track」メニュー、「+ Image」ボタン、セクション名にテクスチャ名を表示 |

### 実装で決めたこと
- 「Pop」は独立したエフェクトにせず、**Zoom ＋ Easing = Back / Bounce** で表現する（重複を避けるため）。
- 「下から上にフェードイン」の作り方:
  - 滑り上がり: Slide ＋ Direction = Bottom ＋ Distance = 40 程度 ＋ Fade ON
  - ワイプ: Wipe ＋ Direction = Bottom ＋ Softness でぼかし幅を指定
- Exit を上書きしない場合は、登場を逆再生する。上書きした場合は、Exit 自身の Easing で順方向に再生する。
- ドラッグで位置を変えたとき、Motion が有効なら、今の時刻に近いほう（開始 or 終了）の位置を、ドロップした場所に来るように逆算して更新する。
  - 元に戻せる操作にした。字幕のドラッグも同様に、元に戻せるようにした。
- ソフトエッジは、テクスチャの UV 範囲を細い帯に分けて、不透明度を段階的に変えて描く（最大 16 段）。
  - 頂点を直接描く API は UE のバージョンによって差があるため、使わない。
- 表示中のテクスチャは UPROPERTY で保持し、GC されないようにした。

### 確認してほしいこと
- UE 5.5〜5.8 でビルドが通るか。特に `FSlateBrush::GetUVRegion` / `SetUVRegion`、`FSlateLayoutTransform`、`FMatrix2x2` まわり。
- 各エフェクトの見た目と、スクラブ・逆再生での動き。
- ソフトエッジの段差が目立たないか。
- 区間の外（隙間）で画像が残らないか。
- エディタ上でのドラッグと Undo、Motion 有効時のドラッグ。
- PIE とパッケージで、サイズや位置がエディタのプレビューと合っているか。

## Phase 2（実装済み・未ビルド。詳細は docs/V1_4_PLAN.md）
- キーフレーム: 位置 X/Y、拡大率、回転、不透明度（`FMovieSceneFloatChannel`。カーブエディタに対応）。
- セクションのバーに、画像のサムネイルを表示する。

## 将来の候補（今回は対象外）
- マテリアル、フリップブック、動画
- ディゾルブ、モザイクなど、マテリアルが必要なエフェクト

## リスク
- ここでは UE をビルドできない。Slate の頂点描画まわり（`FSlateVertex` / `MakeCustomVerts`）は、UE のバージョンによって API が違う可能性がある。5.5〜5.8 でのビルド確認は、ユーザー側でお願いする。
- サンプルのレベルとアセット（.uasset）は、ここでは作れない。
