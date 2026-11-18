<p align="center">
  <img src="resources/branding/ivy_logo.png" width="160" alt="Ivy Engine logo">
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
- **Scripting** — hot-reloadable C++ DLL scripts (`ScriptRuntimeApi` v49), scene JSON authoring, prefab system
- **Debug tools** — time-scale cycling (F7), frame stepping (F12), collision visualizer (F11), live physics stats overlay
- **Asset pipeline** — time-sliced asset preloading (editor overlays + runtime boot loading screen with logo and progress bar), process-lifetime model/animation cache
- **Deploy** — one-click "Build Current Project" packages any content project into a standalone `/MT` exe named after the game, with engine branding overlaid (content can override it)

### Architecture

The engine and game content are fully decoupled:

- This repo = editor (`IvyEngine.exe`) + runtime host (`IvyEngineRuntime.exe`) + read-only [`resources/`](resources) defaults + the in-repo [`testbed/`](testbed) content project
- A **content project** is just a folder: `project.json`, `rom/` assets, `game/scripts/` C++ DLL — created from the editor's New Project flow, or opened from `testbed/` when you only need the sample scenes
- Deployed packages contain only the game: `<GameName>.exe` + assets — the engine itself never appears as a name

### Build

- Visual Studio 2026 (toolset `v145`), Windows SDK, C++20
- Open `IvyEngine.sln` → `IvyEngine` (editor) / `IvyEngineRuntime` (deploy host)
- Runtime builds are statically linked (`/MT`) — the deployed exe has no VC++ redist dependency

### Docs

- **Humans:** [User guide](docs/ENGINE_USER_GUIDE.md) — editor, scenes, scripts, 2D, hot reload, packaging, external content projects
- **AI agents (this repo):** [AGENTS.md](AGENTS.md) — rules, vocabulary, schemas, source map, checks
- **AI agents (game repo):** [Agent docs pack](docs/agent/README.md) — exported with `tools/export_agent_docs.py`
- [API reference](docs/ENGINE_API_REFERENCE.md) (generated) · [Runtime test bridge](docs/RUNTIME_TEST_BRIDGE.md) — Debug named-pipe harness for Runtime and Editor Play mode
- [Architecture overview](docs/diagrams/ivy-engine-architecture.html) · [架構圖（粵）](docs/diagrams/ivy-engine-architecture.zh-HK.html)
- [Architecture decisions](docs/adr/)
- [Engine to-do](docs/TODO.md) — recorded engine backlog

---

## 日本語

**ツタエンジン（Ivy Engine）** は、一人の開発者がゼロから書いた C++ / Direct3D 11 ゲームエンジンです。サードパーティのエンジンコードは一切使っていません。就職ポートフォリオとして開発中。

### 特徴

- **レンダリング** — D3D11 フォワードレンダラ、シェーダ 8 種類：PBR（Cook-Torrance、エネルギー保存則準拠）、トゥーン、フォグ、ポストエフェクト（ブルーム、ビネット、RGB バースト）
- **エディタ** — フル ImGui エディタ：ヒエラルキー、インスペクタ、アセットブラウザ、 transform ギズモ、UI キャンバス、**Undo/Redo**（コマンドパターン、最大 100 ステップ）、ブランド付きスタート画面
- **物理** — AABB / 球 / カプセル / ボックス衝突、トリガーイベント（`OnTriggerEnter/Exit`）、4 種類のレイキャスト、接触可視化
- **並列処理** — 自作ジョブシステム：ワーカープール、依存グラフ、スクリプト並列更新（安全フラグ付き、逐次フォールバックと挙動一致）
- **スクリプト** — ホットリロード対応 C++ DLL スクリプト（`ScriptRuntimeApi` v49）、シーン JSON、プレハブシステム
- **デバッグ** — タイムスケール（F7）、フレームステップ（F12）、コリジョン可視化（F11）、物理統計オーバーレイ
- **アセット** — タイムスライス方式のプリロード（エディタ.overlay + ランタイム起動時のロゴ＆プログレスバー付きローディング画面）、プロセス寿命のモデル/アニメーションキャッシュ
- **デプロイ** — ワンクリック「Build Current Project」で、コンテンツプロジェクトをゲーム名そのままのスタンドアロン `/MT` exe にパッケージ化

### アーキテクチャ

エンジンとゲームコンテンツは完全に分離されています：

- このリポジトリ = エディタ + ランタイムホスト + 読み取り専用の [`resources/`](resources) + リポジトリ内の [`testbed/`](testbed) コンテンツプロジェクト
- **コンテンツプロジェクト**はフォルダ一つ：`project.json`、`rom/` アセット、`game/scripts/` の C++ DLL — エディタの New Project から作成。サンプルシーンだけ試すときは `testbed/` を開く
- デプロイ結果にはゲームだけが入る：`<ゲーム名>.exe` + アセット

### ビルド

- Visual Studio 2026（ツールセット `v145`）、Windows SDK、C++20
- `IvyEngine.sln` を開く → `IvyEngine`（エディタ）/ `IvyEngineRuntime`（デプロイホスト）
- ランタイムはスタティックリンク（`/MT`）— 配布 exe は VC++ 再頒布パッケージ不要

### ドキュメント

- **人間向け:** [ユーザーガイド](docs/ENGINE_USER_GUIDE.md)（英語）— エディタ、シーン、スクリプト、2D、ホットリロード、パッケージ、外部コンテンツプロジェクト
- **AI エージェント向け:** [AGENTS.md](AGENTS.md)（このリポジトリ）· [Agent docs pack](docs/agent/README.md)（ゲームリポジトリ用）
- [API リファレンス](docs/ENGINE_API_REFERENCE.md)（自動生成）· [Runtime test bridge](docs/RUNTIME_TEST_BRIDGE.md) — Debug 専用の named-pipe ハーネス
- [Architecture overview](docs/diagrams/ivy-engine-architecture.html) · [架構圖（粵）](docs/diagrams/ivy-engine-architecture.zh-HK.html)

---

<p align="center"><sub>蔦のように絡みつく、生えるエンジン — Ivy Engine</sub></p>
