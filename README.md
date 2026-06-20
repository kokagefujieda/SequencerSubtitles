# Sequencer Subtitles

![Sequencer Subtitles Demo](https://milkemist.com/sqs/SQS.gif)

**Sequencer Subtitles** は Unreal Engine 5 向けの字幕・台詞表示プラグインです。
Sequencer タイムラインに専用トラックを追加し、リアルタイムプレビュー付きで字幕を配置できます。

**MIT ライセンスで無料配布しています。**
このリポジトリから直接ダウンロードするか、Fab からも無料で入手できます。

> **Fab からダウンロード**: [Sequencer Subtitles on Fab](https://www.fab.com/ja/listings/a5ee3c5a-6aee-4224-841e-2c68880325c8)

## 特徴

- Sequencer に **Subtitle Track** を追加
- エディタ上でのリアルタイムプレビュー
- Widget Blueprint でカスタマイズ可能な UI
- 複数字幕の同時表示（マルチスロット）
- テキストアウトライン（2層・ぼかし対応）
- 文字の揺れ（トレンブル）演出

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
