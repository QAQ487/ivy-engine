<p align="center">
  <img src="rom/branding/ivy_logo.png" width="160" alt="Ivy Engine logo">
</p>

# Ivy Engine

**ツタエンジン** — a from-scratch 3D game engine, built to grow around your game like ivy.

*ゼロから作った 3D ゲームエンジン。蔦のようにゲームコンテンツに絡みつき、育っていく。*

---

## English

**Ivy Engine** is a custom C++ / Direct3D 11 game engine with a full ImGui editor, built as a portfolio project by one developer — every system written from scratch, no third-party engine code.

### Highlights

- **Rendering** — D3D11 forward renderer with 8 shader variants: PBR (Cook-Torrance BRDF, energy-conserving), toon, fog, PostFX pipeline (bloom, vignette, chromatic aberration, RGB burst), cascaded-ready shadow path
- **Editor** — full ImGui editor: hierarchy, inspector, asset browser with context menus, transform gizmos, UI canvas, **undo/redo** (command pattern, 100-deep stack), scene transitions, and a branded start screen
- **Gameplay runtime** — ECS architecture; play mode (F3) runs the exact same components as the deployed game
- **Physics** — AABB / sphere / capsule / box collision, triggers with `OnTriggerEnter/Exit` events, 4-type raycasting, swept resolution, contact visualization
- **Concurrency** — custom job system: worker pool, dependency graph, parallel script updates with per-script safety flags and a sequential fallback that is behavior-identical
- **Scripting** — hot-reloadable C++ DLL scripts (`ScriptRuntimeApi` v40), scene JSON authoring, prefab system
- **Debug tools** — time-scale cycling (F7), frame stepping (F12), collision visualizer (F11), live physics stats overlay
- **Asset pipeline** — time-sliced asset preloading (editor overlays + runtime boot loading screen with logo and progress bar), process-lifetime model/animation cache
- **Deploy** — one-click "Build Current Project" packages any content project into a standalone `/MT` exe named after the game, with engine branding overlaid (content can override it)

### Architecture

The engine and game content are fully decoupled:

- This repo = editor (`IvyEngine.exe`) + runtime host (`IvyEngineRuntime.exe`) + samples
- A **content project** is just a folder: `project.json`, `rom/` assets, `game/scripts/` C++ DLL — created from the editor's New Project flow
- Deployed packages contain only the game: `<GameName>.exe` + assets — the engine itself never appears as a name

### Build

- Visual Studio 2022, Windows SDK, C++17
- Open `IvyEngine.sln` → `IvyEngine` (editor) / `IvyEngineRuntime` (deploy host)
- Runtime builds are statically linked (`/MT`) — the deployed exe has no VC++ redist dependency

### Docs

- [Content Project guide](docs/CONTENT_PROJECT.md) — building games on Ivy
- [Engine manual](docs/ENGINE_MANUAL.md) · [API reference](docs/ENGINE_API_REFERENCE.md) · [User guide](docs/ENGINE_USER_GUIDE.md)
- [Architecture decisions](docs/adr/)

---

## 日本語

**ツタエンジン（Ivy Engine）** は、一人の開発者がゼロから書いた C++ / Direct3D 11 ゲームエンジンです。サードパーティのエンジンコードは一切使っていません。就職ポートフォリオとして開発中。

### 特徴

- **レンダリング** — D3D11 フォワードレンダラ、シェーダ 8 種類：PBR（Cook-Torrance、エネルギー保存則準拠）、トゥーン、フォグ、ポストエフェクト（ブルーム、ビネット、RGB バースト）
- **エディタ** — フル ImGui エディタ：ヒエラルキー、インスペクタ、アセットブラウザ、 transform ギズモ、UI キャンバス、**Undo/Redo**（コマンドパターン、最大 100 ステップ）、ブランド付きスタート画面
- **物理** — AABB / 球 / カプセル / ボックス衝突、トリガーイベント（`OnTriggerEnter/Exit`）、4 種類のレイキャスト、接触可視化
- **並列処理** — 自作ジョブシステム：ワーカープール、依存グラフ、スクリプト並列更新（安全フラグ付き、逐次フォールバックと挙動一致）
- **スクリプト** — ホットリロード対応 C++ DLL スクリプト（`ScriptRuntimeApi` v40）、シーン JSON、プレハブシステム
- **デバッグ** — タイムスケール（F7）、フレームステップ（F12）、コリジョン可視化（F11）、物理統計オーバーレイ
- **アセット** — タイムスライス方式のプリロード（エディタ.overlay + ランタイム起動時のロゴ＆プログレスバー付きローディング画面）、プロセス寿命のモデル/アニメーションキャッシュ
- **デプロイ** — ワンクリック「Build Current Project」で、コンテンツプロジェクトをゲーム名そのままのスタンドアロン `/MT` exe にパッケージ化

### アーキテクチャ

エンジンとゲームコンテンツは完全に分離されています：

- このリポジトリ = エディタ + ランタイムホスト + サンプル
- **コンテンツプロジェクト**はフォルダ一つ：`project.json`、`rom/` アセット、`game/scripts/` の C++ DLL — エディタの New Project から作成
- デプロイ結果にはゲームだけが入る：`<ゲーム名>.exe` + アセット

### ビルド

- Visual Studio 2022、Windows SDK、C++17
- `IvyEngine.sln` を開く → `IvyEngine`（エディタ）/ `IvyEngineRuntime`（デプロイホスト）
- ランタイムはスタティックリンク（`/MT`）— 配布 exe は VC++ 再頒布パッケージ不要

### ドキュメント

- [コンテンツプロジェクトガイド](docs/CONTENT_PROJECT.md)
- [エンジンマニュアル](docs/ENGINE_MANUAL.md) · [API リファレンス](docs/ENGINE_API_REFERENCE.md) · [ユーザーガイド](docs/ENGINE_USER_GUIDE.md)

---

<p align="center"><sub>蔦のように絡みつく、生えるエンジン — Ivy Engine</sub></p>
