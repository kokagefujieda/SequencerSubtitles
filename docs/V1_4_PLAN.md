# v1.4 作業計画（2026-09-30）

## 決定済み（ユーザー回答）

- **対象:**
  - 直す: F1（字幕アニメを時間駆動に）、F2（位置をスロットごとに）、F3（組み込み表示 OFF とイベント）、F4〜F7（小さな修正）
  - 追加: A1（プレイヤー向けの字幕設定）、A3（Image Track の Phase 2）
- **字幕の退場:** 画像と同じく、**セクション内で完了**させる（セクションの最後の Duration 秒で退場する）。
- **対応プラットフォーム:** Win64 のまま。Mac / Linux は確認環境がないので対象外。

## F1: 字幕のアニメーションを時間駆動にする

- **Sequencer の字幕:**
  - eval token が毎フレーム、セクション内の時刻と長さを subsystem に渡す。
  - 登場・退場・トレンブルは、この時刻から計算する（Slate タイマーは使わない）。
- **退場:** セクションの最後の ExitDuration 秒で再生する。
  - 登場と退場の合計がセクションより長い場合は、比率を保って縮める。
- **セクションの外に出たとき（TearDown）:** 即座に消す。
  - 退場はセクション内で終わっているため。途中で停止したときも即座に消える。
- **区間の外（隙間の評価）:** 時刻が範囲外なら消す（Image Track と同じ）。
- **ShowMessage（Sequencer を使わない API）:** subsystem が自前の時計（Slate タイマーで経過時間を加算）を持ち、同じ計算で表示する。
  - 自動で消える時刻は、今と同じ「登場＋Duration＋退場」。
  - HideMessage を呼ぶと、その時点から退場する。
- **イージング:** 既存と同じ EaseOut（指数 2）。退場は登場の逆再生。
- **OnSubtitleEnded:** 字幕が消えた時点で通知する（今は退場の開始時）。

## F2: 表示位置をスロットごとにする

- 位置（上下・左右・ScreenPadding）が同じスロットは、1 つの縦積みグループにまとめる。
  - 全部同じ位置なら、今とまったく同じ見た目になる。
- 位置が違うスロットは、別のグループとして画面の別の場所に出す。
- グループは必要なときに作り、空になったら消す。
- 描画順: 画像（後ろ）→ 字幕 → 画像（前）

## F3: 組み込み表示の OFF と、スロット ID 付きのイベント

- **プロジェクト設定:** `bUseBuiltInDisplay`（既定 true）。
  - false のときは字幕の Slate を作らず、イベントだけを通知する。
  - 自作の UMG で字幕を出す人向け。
- **新しいイベント（BP 用）:** 既存のイベントは互換のため残す。
  - `OnSubtitleSlotStarted(SlotID, Text, SpeakerName, Appearance)`
  - `OnSubtitleSlotTextChanged(SlotID, VisibleText)`（タイプライターの途中経過）
  - `OnSubtitleSlotEnded(SlotID)`
- README の「Widget Blueprint でカスタマイズ可能」を、実際の仕組みに合わせて書き直す。

## F4〜F7: 小さな修正

| # | 内容 |
|---|---|
| F4 | エディタでのドラッグを、プロジェクト設定 `bEnableViewportDrag`（既定 true）で ON/OFF できるようにする。画像の Fit Screen / Fill Screen は常にドラッグ対象外（クリックを通す） |
| F5 | 字幕のウィンドウ画像と区切り線の画像を UPROPERTY で保持して、GC を防ぐ |
| F6 | タイプライター中の文字の塊の位置を、TextAlignment（左・中央・右）に合わせる |
| F7 | クリップボードの自動貼り付けを、設定 `bPasteClipboardOnAddSection`（既定 true）で OFF にできるようにする。`ClipboardPasteMaxChars`（既定 300）より長い場合は貼り付けない |

## A1: プレイヤー向けの字幕設定

- **保存先:** `USubtitleUserSettings`（Config = GameUserSettings）。プレイヤーごとの GameUserSettings.ini に保存する。
- **Blueprint 関数**（Function Library）:
  - `SetSubtitlesEnabled` / `AreSubtitlesEnabled`
  - `SetSubtitleTextScale` / `GetSubtitleTextScale`（0.5〜3.0。本文・話者名・アウトラインに掛かる）
  - `SetSubtitleBackgroundOpacity(bOverride, Opacity)` / `GetSubtitleBackgroundOpacity`
    - プレイヤーが背景の濃さを指定できる。作品側で背景が透明でも、プレイヤーが背景を付けられる。
  - `SaveSubtitleUserSettings`
- 変更は、表示中の字幕にもすぐに反映する。

## A3: Image Track の Phase 2

- **キーフレーム:** セクションに 5 つのチャンネルを追加し、Sequencer のカーブエディタで編集できるようにする。
  - Offset X / Offset Y、Scale、Rotation、Opacity（下の Q4 を参照）
- **サムネイル:** セクションのバーの右端に、画像のサムネイルを描く。セクションの高さも少し上げる。
- **ドラッグ＆ドロップ:** コンテンツブラウザからテクスチャを Sequencer にドロップすると、Image Track に再生ヘッドの位置からセクションを作る。
  - Image Track がなければ作る。
  - 使うのは `HandleAssetAdded`（オーディオトラックと同じ仕組み）。

## 決定事項（Q1〜Q4 は、すべて既定案で回答済み）

- Q1: 字幕を OFF にしても、ShowMessage は**消さない**。
- Q2: 字幕を OFF にしても、イベントは**通知する**。
- Q3: エンジン標準の字幕 ON/OFF と**連動させる**。
  - `SetSubtitlesEnabled` を呼ぶと `GEngine->bSubtitlesEnabled` も切り替える。
  - 表示するのは、両方が ON のときだけ。
  - 起動時にこちらからエンジン側を書き換えることはしない（ゲーム独自の設定を上書きしないため）。
- Q4: キーフレームは**重ねる**。Offset と Rotation は加算、Scale と Opacity は乗算。

## 実装状況（コミット済み・**未ビルド**）

| 項目 | 状態 | 補足 |
|---|---|---|
| F1 | 済 | eval token が毎フレーム時刻を渡し、`ApplySubtitleVisual` で計算する。ShowMessage は `FTSTicker` で時計を進める。タイプライターは退場の前に打ち終わるように調整した |
| F2 | 済 | `SubtitleGroups`（キー＝位置＋パディング）。WidgetOverlay の Z 順は 画像(後)0 / 字幕 1 / 画像(前)2 |
| F3 | 済 | `bUseBuiltInDisplay`、`OnSubtitleSlotStarted` / `OnSubtitleSlotTextChanged` / `OnSubtitleSlotEnded`。README を修正 |
| F4 | 済 | `bEnableViewportDrag`。ドラッグハンドルはエディタのビューポートでだけ作る。**PIE では字幕がクリックを吸わなくなった**（CinematicADV のクリック送りの妨げになる可能性があったため） |
| F5 | 済 | `SlotWindowTextures` / `SlotLineTextures` |
| F6 | 済 | `RebuildTypewriterSizer` で TextAlignment に合わせる |
| F7 | 済 | `bPasteClipboardOnAddSection` / `ClipboardPasteMaxChars` |
| A1 | 済 | `USubtitleUserSettings`（GameUserSettings.ini）と `USubtitleUserSettingsLibrary`。変更は表示中の字幕にもすぐ反映する |
| A3 | 済 | チャンネル 5 本（`CacheChannelProxy`）、サムネイル（セクションの高さ 40）、`HandleAssetAdded` |

### 実装で決めたこと
- タイプライターの効果音は、プラグイン自身が字幕を表示しているときだけ鳴らす（組み込み表示 OFF や、字幕 OFF のときは鳴らさない）。
- `OnSubtitleSlotStarted` で渡す Appearance は、作者が設定した値（プレイヤー設定を適用する前）。自作 UI 側で `GetSubtitleTextScale` などを使う想定。
- `OnSubtitleEnded` は、字幕が消えた時点で通知する（以前は退場の開始時）。
- ShowMessage を HideMessage で途中で消す場合は、その時点から退場する。登場の途中なら、登場と退場のうち不透明度が低いほうを採る。
- 旧 API（スロット ID なしの `NotifySubtitleStarted`）は、subsystem の時計で動き、`NotifySubtitleEnded()` で退場する。
- 字幕 OFF・ON の切り替えは、表示中の字幕にもすぐ反映する（非表示にするだけで、状態は保持する）。

### 確認してほしいこと
- ビルド（特に `CacheChannelProxy`、`FMovieSceneChannelMetaData`、`TMovieSceneExternalValue`、`HandleAssetAdded`、`FTSTicker`）
- 字幕: スクラブ・逆再生での登場と退場、退場がセクション内で終わること、MRQ での書き出し、上下同時の表示、PIE でのクリック
- ShowMessage: 自動で消えるタイミングと HideMessage
- プレイヤー設定: 文字サイズ・背景の濃さ・ON/OFF がすぐ反映され、保存されること
- 画像: キーフレーム（カーブエディタ）、サムネイル、コンテンツブラウザからのドロップ、全画面の画像でもビューポートを操作できること

## リスク
- F1 と F2 は字幕の中核の作り替えになる。ビルドと動作の確認は、ユーザー側でお願いする。
- チャンネル API（`CacheChannelProxy` / `FMovieSceneChannelProxyData::Add`）と `HandleAssetAdded` は、UE 5.5〜5.8 でシグネチャが違う可能性がある。
