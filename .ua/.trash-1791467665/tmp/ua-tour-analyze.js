#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");

function die(msg) {
  process.stderr.write(String(msg) + "\n");
  process.exit(1);
}

function loadJson(p) {
  try {
    return JSON.parse(fs.readFileSync(p, "utf8"));
  } catch (e) {
    die(`Failed to read JSON ${p}: ${e.message}`);
  }
}

function normalizePath(p) {
  return String(p || "").replace(/\\/g, "/");
}

function basename(p) {
  const n = normalizePath(p);
  const i = n.lastIndexOf("/");
  return i >= 0 ? n.slice(i + 1) : n;
}

function depthFromRoot(filePath) {
  const n = normalizePath(filePath).replace(/^\.\//, "");
  if (!n) return 0;
  return n.split("/").filter(Boolean).length - 1;
}

const ENTRY_FILENAMES = new Set([
  "index.ts", "index.js", "main.ts", "main.js", "app.ts", "app.js",
  "server.ts", "server.js", "mod.rs", "main.go", "main.py", "main.rs",
  "manage.py", "app.py", "wsgi.py", "asgi.py", "run.py", "__main__.py",
  "Application.java", "Main.java", "Program.cs", "config.ru", "index.php",
  "App.swift", "Application.kt", "main.cpp", "main.c",
]);

const DOC_TYPES = new Set(["document"]);
const INFRA_TYPES = new Set(["service", "pipeline", "resource"]);
const DATA_TYPES = new Set(["table", "schema", "endpoint"]);
const CONFIG_TYPES = new Set(["config"]);

function main() {
  const inputPath = process.argv[2];
  const outputPath = process.argv[3];
  if (!inputPath || !outputPath) {
    die("Usage: node ua-tour-analyze.js <input.json> <output.json>");
  }

  const input = loadJson(inputPath);
  const nodes = Array.isArray(input.nodes) ? input.nodes : [];
  const edges = Array.isArray(input.edges) ? input.edges : [];
  const layersIn = Array.isArray(input.layers) ? input.layers : [];

  const nodeById = new Map();
  for (const n of nodes) {
    if (n && n.id) nodeById.set(n.id, n);
  }

  const fanIn = new Map();
  const fanOut = new Map();
  for (const id of nodeById.keys()) {
    fanIn.set(id, 0);
    fanOut.set(id, 0);
  }

  const importCallAdj = new Map();
  const pairEdges = new Map();
  const directedPairs = new Set();

  function addAdj(src, tgt) {
    if (!importCallAdj.has(src)) importCallAdj.set(src, new Set());
    importCallAdj.get(src).add(tgt);
  }

  function pairKey(a, b) {
    return a < b ? `${a}||${b}` : `${b}||${a}`;
  }

  for (const e of edges) {
    if (!e || !e.source || !e.target) continue;
    const srcKnown = nodeById.has(e.source);
    const tgtKnown = nodeById.has(e.target);
    if (srcKnown && tgtKnown && e.source !== e.target) {
      fanOut.set(e.source, fanOut.get(e.source) + 1);
      fanIn.set(e.target, fanIn.get(e.target) + 1);
      const k = pairKey(e.source, e.target);
      pairEdges.set(k, (pairEdges.get(k) || 0) + 1);
      directedPairs.add(`${e.source}=>${e.target}::${e.type || ""}`);
    } else if (srcKnown && !tgtKnown) {
      fanOut.set(e.source, fanOut.get(e.source) + 1);
    } else if (tgtKnown && !srcKnown) {
      fanIn.set(e.target, fanIn.get(e.target) + 1);
    }

    if (
      srcKnown &&
      tgtKnown &&
      e.source !== e.target &&
      (e.type === "imports" || e.type === "calls")
    ) {
      addAdj(e.source, e.target);
    }
  }

  function rankMap(map) {
    return [...map.entries()]
      .map(([id, count]) => ({
        id,
        count,
        name: (nodeById.get(id) && nodeById.get(id).name) || id,
      }))
      .sort((a, b) => b.count - a.count || a.id.localeCompare(b.id));
  }

  const fanInSorted = rankMap(fanIn);
  const fanOutSorted = rankMap(fanOut);

  const fanInRanking = fanInSorted.slice(0, 20).map((x) => ({
    id: x.id,
    fanIn: x.count,
    name: x.name,
  }));
  const fanOutRanking = fanOutSorted.slice(0, 20).map((x) => ({
    id: x.id,
    fanOut: x.count,
    name: x.name,
  }));

  const fanOutValues = fanOutSorted.map((x) => x.count);
  const fanInValues = fanInSorted.map((x) => x.count);
  const fanOutThreshold = percentile(fanOutValues, 0.9);
  const fanInLowThreshold = percentile(fanInValues, 0.25);

  function percentile(sortedDescValues, p) {
    if (!sortedDescValues.length) return 0;
    const asc = [...sortedDescValues].sort((a, b) => a - b);
    const idx = Math.min(asc.length - 1, Math.max(0, Math.ceil(p * asc.length) - 1));
    return asc[idx];
  }

  const scored = [];
  for (const n of nodes) {
    let score = 0;
    const filePath = normalizePath(n.filePath || n.name || "");
    const fileName = basename(filePath);
    const type = n.type || "file";

    if (type === "document" || fileName.toLowerCase().endsWith(".md")) {
      if (fileName.toLowerCase() === "readme.md" && depthFromRoot(filePath) === 0) {
        score += 5;
      } else if (fileName.toLowerCase().endsWith(".md") && depthFromRoot(filePath) === 0) {
        score += 2;
      }
    } else {
      if (ENTRY_FILENAMES.has(fileName)) score += 3;
      const depth = depthFromRoot(filePath);
      if (depth <= 1) score += 1;
      const fo = fanOut.get(n.id) || 0;
      const fi = fanIn.get(n.id) || 0;
      if (fo >= fanOutThreshold && fanOutValues.length) score += 1;
      if (fi <= fanInLowThreshold) score += 1;
    }

    if (score > 0) {
      scored.push({
        id: n.id,
        score,
        name: n.name,
        summary: n.summary || "",
        type,
      });
    }
  }

  scored.sort((a, b) => b.score - a.score || a.id.localeCompare(b.id));
  const entryPointCandidates = scored.slice(0, 5).map(({ type, ...rest }) => rest);

  let startNode = null;
  for (const c of scored) {
    const n = nodeById.get(c.id);
    if (n && n.type !== "document") {
      startNode = c.id;
      break;
    }
  }
  if (!startNode) {
    const codeNodes = nodes.filter((n) => n.type === "file");
    if (codeNodes.length) startNode = codeNodes[0].id;
  }

  const order = [];
  const depthMap = {};
  const byDepth = {};
  if (startNode && nodeById.has(startNode)) {
    const visited = new Set([startNode]);
    const queue = [startNode];
    depthMap[startNode] = 0;
    while (queue.length) {
      const cur = queue.shift();
      order.push(cur);
      const d = depthMap[cur];
      const key = String(d);
      if (!byDepth[key]) byDepth[key] = [];
      byDepth[key].push(cur);
      const nexts = [...(importCallAdj.get(cur) || [])].sort();
      for (const nxt of nexts) {
        if (visited.has(nxt)) continue;
        visited.add(nxt);
        depthMap[nxt] = d + 1;
        queue.push(nxt);
      }
    }
  }

  const documentation = [];
  const infrastructure = [];
  const data = [];
  const config = [];
  for (const n of nodes) {
    const item = {
      id: n.id,
      name: n.name,
      type: n.type,
      summary: n.summary || "",
    };
    if (DOC_TYPES.has(n.type)) documentation.push(item);
    else if (INFRA_TYPES.has(n.type)) infrastructure.push(item);
    else if (DATA_TYPES.has(n.type)) data.push(item);
    else if (CONFIG_TYPES.has(n.type)) config.push(item);
  }

  const bidirectional = new Set();
  for (const e of edges) {
    if (!e || !nodeById.has(e.source) || !nodeById.has(e.target)) continue;
    if (e.source === e.target) continue;
    if (e.type !== "imports" && e.type !== "calls") continue;
    const reverse = `${e.target}=>${e.source}::${e.type}`;
    const forward = `${e.source}=>${e.target}::${e.type}`;
    if (directedPairs.has(reverse) && directedPairs.has(forward)) {
      bidirectional.add(pairKey(e.source, e.target));
    }
  }

  const clusters = [];
  const used = new Set();
  for (const key of bidirectional) {
    const [a, b] = key.split("||");
    if (used.has(a) || used.has(b)) continue;
    const cluster = new Set([a, b]);
    let grew = true;
    while (grew && cluster.size < 5) {
      grew = false;
      const candidates = new Map();
      for (const e of edges) {
        if (!e || !nodeById.has(e.source) || !nodeById.has(e.target)) continue;
        if (e.source === e.target) continue;
        const srcIn = cluster.has(e.source);
        const tgtIn = cluster.has(e.target);
        if (srcIn === tgtIn) continue;
        const outsider = srcIn ? e.target : e.source;
        if (used.has(outsider)) continue;
        candidates.set(outsider, (candidates.get(outsider) || 0) + 1);
      }
      const addable = [...candidates.entries()]
        .filter(([, c]) => c >= 2)
        .sort((x, y) => y[1] - x[1] || x[0].localeCompare(y[0]));
      if (addable.length && cluster.size < 5) {
        cluster.add(addable[0][0]);
        grew = true;
      }
    }
    let edgeCount = 0;
    const members = [...cluster];
    for (let i = 0; i < members.length; i++) {
      for (let j = i + 1; j < members.length; j++) {
        edgeCount += pairEdges.get(pairKey(members[i], members[j])) || 0;
      }
    }
    for (const id of members) used.add(id);
    clusters.push({ nodes: members.sort(), edgeCount });
  }
  clusters.sort((a, b) => b.edgeCount - a.edgeCount || b.nodes.length - a.nodes.length);
  const topClusters = clusters.slice(0, 10);

  const nodeSummaryIndex = {};
  for (const n of nodes) {
    nodeSummaryIndex[n.id] = {
      name: n.name,
      type: n.type,
      summary: n.summary || "",
    };
  }

  const result = {
    scriptCompleted: true,
    entryPointCandidates,
    fanInRanking,
    fanOutRanking,
    bfsTraversal: {
      startNode: startNode || "",
      order,
      depthMap,
      byDepth,
    },
    nonCodeFiles: {
      documentation,
      infrastructure,
      data,
      config,
    },
    clusters: topClusters,
    layers: {
      count: layersIn.length,
      list: layersIn.map((l) => ({
        id: l.id,
        name: l.name,
        description: l.description || "",
      })),
    },
    nodeSummaryIndex,
    totalNodes: nodes.length,
    totalEdges: edges.length,
  };

  fs.mkdirSync(path.dirname(path.resolve(outputPath)), { recursive: true });
  fs.writeFileSync(outputPath, JSON.stringify(result, null, 2));
}

try {
  main();
} catch (e) {
  die(e && e.stack ? e.stack : String(e));
}
