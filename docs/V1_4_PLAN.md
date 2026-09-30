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

## 確認したい点（既定案）

- Q1: 字幕 OFF のとき、ShowMessage も消すか → 既定案: **消さない**（ShowMessage は台詞以外のメッセージにも使えるため）
- Q2: 字幕 OFF のとき、イベント（OnSubtitleStarted など）を通知するか → 既定案: **通知する**（バックログなどのゲーム側の処理が止まらないように）
- Q3: エンジン標準の字幕 ON/OFF（`GEngine->bSubtitlesEnabled`）と連動させるか → 既定案: **連動する**（Set 時に同期する）
- Q4: キーフレームの効き方 → 既定案: **Layout の設定に重ねる**（Offset は加算、Scale と Opacity は乗算、Rotation は加算）。キーがなければ何もしない

## リスク
- F1 と F2 は字幕の中核の作り替えになる。ビルドと動作の確認は、ユーザー側でお願いする。
- チャンネル API（`CacheChannelProxy` / `FMovieSceneChannelProxyData::Add`）と `HandleAssetAdded` は、UE 5.5〜5.8 でシグネチャが違う可能性がある。
