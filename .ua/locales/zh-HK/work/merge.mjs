import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";

const repo = "C:/Users/terry/source/repos/ivy-engine";
const work = path.join(repo, ".ua/locales/zh-HK/work");
const outFile = path.join(repo, ".ua/locales/zh-HK/knowledge-graph.json");
const viewerFile = "C:/Users/terry/source/repos/ivy-engine-zh-HK-view/.ua/knowledge-graph.json";

const read = (p) => JSON.parse(fs.readFileSync(p, "utf8").replace(/^\uFEFF/, ""));
const graph = read(path.join(repo, ".ua/knowledge-graph.json"));
const meta = read(path.join(work, "meta-out.json"));

const tr = {};
for (const name of fs.readdirSync(work).filter((f) => /^out-.+\.json$/.test(f)).sort()) {
  Object.assign(tr, read(path.join(work, name)));
}

let missing = 0;
graph.nodes = graph.nodes.map((n) => {
  const t = tr[n.id];
  if (!t || !t.summary) { missing++; return n; }
  const out = { ...n, summary: t.summary };
  if (n.languageNotes && t.languageNotes) out.languageNotes = t.languageNotes;
  return out;
});

graph.project = { ...graph.project, description: meta.project };
graph.layers = graph.layers.map((l) => ({ ...l, ...(meta.layers[l.id] ?? {}) }));
graph.tour = graph.tour.map((s) => ({ ...s, ...(meta.tour[String(s.order)] ?? {}) }));

const json = JSON.stringify(graph, null, 2);
fs.writeFileSync(outFile, json, "utf8");
console.log(`nodes=${graph.nodes.length} translated=${graph.nodes.length - missing} missing=${missing}`);

// The viewer is a git worktree; keep its checkout on the graph's commit so the
// dashboard freshness check compares against the same revision.
const viewerRoot = path.dirname(path.dirname(viewerFile));
if (fs.existsSync(path.join(viewerRoot, ".git"))) {
  execFileSync("git", ["-C", viewerRoot, "checkout", "--detach", "--quiet", graph.project.gitCommitHash]);
  fs.mkdirSync(path.dirname(viewerFile), { recursive: true });
  fs.writeFileSync(viewerFile, json, "utf8");
  fs.copyFileSync(path.join(repo, ".ua/meta.json"), path.join(viewerRoot, ".ua/meta.json"));
  fs.writeFileSync(path.join(viewerRoot, ".ua/config.json"), '{"outputLanguage":"zh-TW"}', "utf8");
  console.log(`viewer updated at ${graph.project.gitCommitHash.slice(0, 7)}`);
} else {
  console.error(`viewer worktree missing at ${viewerRoot}`);
}
