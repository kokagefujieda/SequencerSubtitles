# Sequencer Subtitles

![Sequencer Subtitles Demo](https://milkemist.com/sqs/SQS.gif)

**Sequencer Subtitles** は Unreal Engine 5 向けの字幕・台詞表示プラグインです。
Sequencer タイムラインに専用トラックを追加し、リアルタイムプレビュー付きで字幕を配置できます。

**MIT ライセンスで無料配布しています。**
このリポジトリから直接ダウンロードするか、Fab からも無料で入手できます。

> **Fab からダウンロード**: [Sequencer Subtitles on Fab](https://www.fab.com/ja/listings/a5ee3c5a-6aee-4224-841e-2c68880325c8)

## 特徴

- Sequencer に **Subtitle Track** を追加
- エディタ上でのリアルタイムプレビュー（ビューポートでドラッグして位置調整）
- フォント・アウトライン・メッセージウィンドウ・登場／退場アニメーションを詳細設定でカスタマイズ
- Blueprint イベント（スロット ID 付き）で自作 UMG にも対応（組み込み表示は設定で OFF 可）
- 複数字幕の同時表示（マルチスロット。位置の違う字幕も同時に表示可）
- テキストアウトライン（2層・ぼかし対応）
- 文字の揺れ（トレンブル）演出
- **Image Track**: 画像を登場／退場エフェクト付きで表示（フェード、スライド、ワイプ、分割合体、アイリスなど）
- プレイヤー向けの字幕設定（表示 ON/OFF・文字サイズ・背景の濃さ）
- アニメーションはシーケンスの時間に同期（スクラブ・Movie Render Queue でも正確）

![Sequencer Subtitles Feature](https://milkemist.com/sqs/SQS2.gif)

## 動作環境

- Unreal Engine 5.5 / 5.6 / 5.7 / 5.8

## インストール

1. このページの [Releases](https://github.com/kokagefujieda/SequencerSubtitles/releases) から UE バージョンに合った zip をダウンロード
2. `SequencerSubtitles` フォルダをプロジェクトの `Plugins/` にコピー
3. UE エディタを起動し、プラグインを有効化
4. Sequencer を開き、`+ Track` → **Subtitle Track** を追加

## ドキュメント

**[Sequencer Subtitles 公式ドキュメント](https://milkemist.com/sqs/)**

## 更新履歴

### 次のバージョン（未リリース）
- Image Track を追加（エフェクト、キーフレーム、サムネイル、コンテンツブラウザからのドロップ）
- 字幕のアニメーションをシーケンスの時間に同期。退場はセクションの終わりの中で再生するように変更
- PIE / ゲームで字幕の DPI スケールが二重にかかっていた問題を修正（1080p 以外で表示サイズが変わります）
- 位置の違う字幕を同時に表示できるように
- MessageWindowHeight を「最小の高さ」に変更（文字に合わせてウィンドウが広がる）
- テキスト編集で見た目の設定が既定値に戻る問題を修正（一括で戻すメニューを追加）
- プレイヤー向けの字幕設定、組み込み表示の OFF 設定、スロット ID 付きイベントを追加

### v1.3
- UE 5.8 に対応
- 複数字幕の同時表示（マルチスロット）に対応
- テキストアウトライン（2層・ぼかし対応）を追加
- 文字の揺れ（トレンブル）演出を追加
- `ShowMessage` / `ShowPersistentMessage` などの Blueprint API を拡充

### v1.2
- 初回 Fab 公開版

## ライセンス

[MIT License](LICENSE)
