# Claude 向けメモ（SequencerSubtitles）

## 「Claude でオンラインでいじった前のところまで戻して」と言われたら

- **戻す先:** Claude Code（オンライン）で編集する前の公開版。
  - タグ **`v1.3`** ＝ コミット **`0ef8ef1`**。2026-09-30 時点の `main` と同じ。
- **前提:** Claude の変更はすべて `claude/*` ブランチで行い、2026-09-30 時点では `main` にマージしていない。

### 戻し方

1. **状態を確認する**
   - `git fetch origin main --tags`
   - `git log --oneline v1.3..origin/main`
2. **何も出ない場合**
   - `main` はすでに公開版のまま。そのことをユーザーに伝える。
   - 作業ブランチを消すかどうかは、ユーザーに確認する。
3. **マージ済みの場合**
   - 履歴は消さない。公開版の中身に戻すコミットを新しく作る。
     ```sh
     git restore --source=v1.3 --staged --worktree -- :/
     git commit -m "revert: Claude で編集する前の公開版（v1.3）に戻す"
     ```
   - 作業ブランチに push して PR にする。
4. **禁止事項:** `main` への force-push、履歴の書き換え、タグの付け替え。
   - どれも、必ずユーザーに確認してから。

## その他

- レビュー内容、決定事項、没にした案は `docs/REVIEW_NOTES.md` にある。
