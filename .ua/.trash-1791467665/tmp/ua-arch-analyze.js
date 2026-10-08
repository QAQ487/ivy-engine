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

function dirname(p) {
  const n = normalizePath(p);
  const i = n.lastIndexOf("/");
  return i >= 0 ? n.slice(0, i) : "";
}

function commonPrefix(paths) {
  if (!paths.length) return "";
  const parts = paths.map((p) => normalizePath(p).split("/").filter(Boolean));
  const minLen = Math.min(...parts.map((x) => x.length));
  const prefix = [];
  for (let i = 0; i < minLen; i++) {
    const seg = parts[0][i];
    if (parts.every((x) => x[i] === seg) && parts.every((x) => x.length > i + 1 || false)) {
      // only keep as directory prefix if ALL paths have a remaining segment after this
      const allHaveMore = parts.every((x) => x.length > i + 1);
      if (!allHaveMore) break;
      prefix.push(seg);
    } else {
      break;
    }
  }
  return prefix.length ? prefix.join("/") + "/" : "";
}

const DIR_PATTERNS = [
  [["routes", "api", "controllers", "endpoints", "handlers", "controller", "routers", "serializers", "blueprints"], "api"],
  [["services", "core", "lib", "domain", "logic", "internal", "signals", "mailers", "jobs", "channels", "composables"], "service"],
  [["models", "db", "data", "persistence", "repository", "entities", "migrations", "entity", "sql", "database", "schema"], "data"],
  [["components", "views", "pages", "ui", "layouts", "screens"], "ui"],
  [["middleware", "plugins", "interceptors", "guards"], "middleware"],
  [["utils", "helpers", "common", "shared", "tools", "pkg", "templatetags"], "utility"],
  [["config", "constants", "env", "settings", "management", "commands"], "config"],
  [["__tests__", "test", "tests", "spec", "specs"], "test"],
  [["types", "interfaces", "schemas", "contracts", "dtos", "dto", "request", "response"], "types"],
  [["hooks"], "hooks"],
  [["store", "state", "reducers", "actions", "slices"], "state"],
  [["assets", "static", "public"], "assets"],
  [["cmd", "bin"], "entry"],
  [["docs", "documentation", "wiki"], "documentation"],
  [["deploy", "deployment", "infra", "infrastructure", "k8s", "kubernetes", "helm", "charts", "terraform", "tf", "docker"], "infrastructure"],
  [[".github", ".gitlab", ".circleci"], "ci-cd"],
];

function matchDirPattern(name) {
  const lower = String(name || "").toLowerCase();
  for (const [names, label] of DIR_PATTERNS) {
    if (names.includes(lower)) return label;
  }
  return null;
}

function filePatternLabel(filePath, nodeType) {
  const p = normalizePath(filePath);
  const name = basename(p);
  const lower = name.toLowerCase();
  const dir = dirname(p);

  if (/\.(test|spec)\./i.test(name) || /^test_.*\.py$/i.test(name) || /_test\.go$/i.test(name) ||
      /Test\.java$/.test(name) || /_spec\.rb$/i.test(name) || /Test\.php$/.test(name) || /Tests\.cs$/.test(name)) {
    return "test";
  }
  if (/\.d\.ts$/i.test(name)) return "types";
  if (/^(index\.ts|index\.js|__init__\.py)$/i.test(name) && dir && !dir.includes("/")) return "entry";
  if (lower === "manage.py" && !dir) return "entry";
  if (lower === "wsgi.py" || lower === "asgi.py") return "config";
  if (lower === "main.go" && /(^|\/)cmd\//.test(p)) return "entry";
  if ((lower === "main.rs" || lower === "lib.rs") && /(^|\/)src\//.test(p)) return "entry";
  if (name === "Application.java" || name === "Program.cs") return "entry";
  if (lower === "config.ru") return "entry";
  if (["cargo.toml", "go.mod", "gemfile", "pom.xml", "build.gradle", "composer.json"].includes(lower)) return "config";
  if (lower === "dockerfile" || /^docker-compose/i.test(name)) return "infrastructure";
  if (/\.tf$|\.tfvars$/i.test(name)) return "infrastructure";
  if (/(^|\/)\.github\/workflows\//.test(p) || lower === ".gitlab-ci.yml" || name === "Jenkinsfile") return "ci-cd";
  if (/\.sql$/i.test(name)) return "data";
  if (/\.(graphql|gql|proto)$/i.test(name)) return "types";
  if (/\.(md|rst)$/i.test(name)) return "documentation";
  if (name === "Makefile") return "infrastructure";
  if (nodeType === "document") return "documentation";
  if (nodeType === "config") return "config";
  if (nodeType === "pipeline") return "ci-cd";
  if (nodeType === "service" || nodeType === "resource") return "infrastructure";
  if (nodeType === "table" || nodeType === "schema") return "data";
  return null;
}

function detectDeployment(fileNodes) {
  const paths = fileNodes.map((n) => normalizePath(n.filePath || n.name || ""));
  const names = fileNodes.map((n) => basename(n.filePath || n.name || "").toLowerCase());
  const infraFiles = [];
  let hasDockerfile = false, hasCompose = false, hasK8s = false, hasTerraform = false, hasCI = false;
  for (const n of fileNodes) {
    const p = normalizePath(n.filePath || n.name || "");
    const name = basename(p);
    const lower = name.toLowerCase();
    let isInfra = false;
    if (lower === "dockerfile" || lower.startsWith("dockerfile.")) { hasDockerfile = true; isInfra = true; }
    if (/^docker-compose/i.test(name)) { hasCompose = true; isInfra = true; }
    if (/\.(tf|tfvars)$/i.test(name) || /(^|\/)terraform\//.test(p)) { hasTerraform = true; isInfra = true; }
    if (/(^|\/)(k8s|kubernetes|helm|charts)\//.test(p) || /\.ya?ml$/i.test(name) && /k8s|kube|helm/i.test(p)) { hasK8s = true; isInfra = true; }
    if (/(^|\/)\.github\/workflows\//.test(p) || lower === ".gitlab-ci.yml" || name === "Jenkinsfile" || n.type === "pipeline") { hasCI = true; isInfra = true; }
    if (n.type === "service" || n.type === "resource") isInfra = true;
    if (isInfra) infraFiles.push(p || n.id);
  }
  return { hasDockerfile, hasCompose, hasK8s, hasTerraform, hasCI, infraFiles };
}

function detectDataPipeline(fileNodes) {
  const schemaFiles = [];
  const migrationFiles = [];
  const dataModelFiles = [];
  const apiHandlerFiles = [];
  for (const n of fileNodes) {
    const p = normalizePath(n.filePath || "");
    const tags = (n.tags || []).map((t) => String(t).toLowerCase());
    if (/\.(sql|graphql|gql|proto|prisma)$/i.test(p) || n.type === "schema") schemaFiles.push(p);
    if (/(^|\/)migrations\//.test(p) || /\.sql$/i.test(p)) migrationFiles.push(p);
    if (/(^|\/)(models|entities|entity)\//.test(p) || tags.includes("data-model") || tags.includes("model")) dataModelFiles.push(p);
    if (/(^|\/)(routes|api|controllers|handlers|endpoints)\//.test(p) || tags.includes("api-handler") || n.type === "endpoint") apiHandlerFiles.push(p);
  }
  return { schemaFiles, migrationFiles, dataModelFiles, apiHandlerFiles };
}

function main() {
  const inPath = process.argv[2];
  const outPath = process.argv[3];
  if (!inPath || !outPath) die("Usage: node ua-arch-analyze.js <input.json> <output.json>");

  const input = loadJson(inPath);
  const fileNodes = Array.isArray(input.fileNodes) ? input.fileNodes : [];
  const importEdges = Array.isArray(input.importEdges) ? input.importEdges : [];
  const allEdges = Array.isArray(input.allEdges) ? input.allEdges : [];
  if (!fileNodes.length) die("No fileNodes in input");

  const nodeById = new Map();
  for (const n of fileNodes) {
    if (!n || !n.id) continue;
    nodeById.set(n.id, n);
  }

  const paths = fileNodes.map((n) => normalizePath(n.filePath || "")).filter(Boolean);
  const prefix = commonPrefix(paths);

  const directoryGroups = {};
  function groupKeyFor(n) {
    const p = normalizePath(n.filePath || "");
    let rest = p;
    if (prefix && rest.startsWith(prefix)) rest = rest.slice(prefix.length);
    const segs = rest.split("/").filter(Boolean);
    if (segs.length <= 1) {
      const name = segs[0] || basename(p) || n.name || n.id;
      if (/\.(test|spec)\./i.test(name) || /^test_/i.test(name)) return "test";
      if (/\.config\./i.test(name) || /\.(json|yml|yaml|toml|ini)$/i.test(name)) return "config-root";
      if (!rest.includes("/")) return "root";
      return segs[0] || "root";
    }
    return segs[0];
  }

  const idToGroup = {};
  for (const n of fileNodes) {
    const g = groupKeyFor(n);
    if (!directoryGroups[g]) directoryGroups[g] = [];
    directoryGroups[g].push(n.id);
    idToGroup[n.id] = g;
  }

  const nodeTypeGroups = {};
  for (const n of fileNodes) {
    const t = n.type || "file";
    if (!nodeTypeGroups[t]) nodeTypeGroups[t] = [];
    nodeTypeGroups[t].push(n.id);
  }

  const fileFanOut = {};
  const fileFanIn = {};
  const adj = {};
  for (const n of fileNodes) {
    fileFanOut[n.id] = 0;
    fileFanIn[n.id] = 0;
    adj[n.id] = [];
  }
  for (const e of importEdges) {
    if (!e || !e.source || !e.target) continue;
    if (!nodeById.has(e.source) || !nodeById.has(e.target)) continue;
    fileFanOut[e.source] = (fileFanOut[e.source] || 0) + 1;
    fileFanIn[e.target] = (fileFanIn[e.target] || 0) + 1;
    if (!adj[e.source]) adj[e.source] = [];
    adj[e.source].push(e.target);
  }

  const interCount = {};
  const intraInternal = {};
  const groupTotalEdges = {};
  for (const g of Object.keys(directoryGroups)) {
    intraInternal[g] = 0;
    groupTotalEdges[g] = 0;
  }
  for (const e of importEdges) {
    if (!e || !e.source || !e.target) continue;
    const fg = idToGroup[e.source];
    const tg = idToGroup[e.target];
    if (!fg || !tg) continue;
    groupTotalEdges[fg] = (groupTotalEdges[fg] || 0) + 1;
    groupTotalEdges[tg] = (groupTotalEdges[tg] || 0) + 1;
    if (fg === tg) {
      intraInternal[fg] = (intraInternal[fg] || 0) + 1;
    } else {
      const k = fg + "\0" + tg;
      interCount[k] = (interCount[k] || 0) + 1;
    }
  }
  const interGroupImports = Object.entries(interCount)
    .map(([k, count]) => {
      const [from, to] = k.split("\0");
      return { from, to, count };
    })
    .sort((a, b) => b.count - a.count);

  const intraGroupDensity = {};
  for (const g of Object.keys(directoryGroups)) {
    const internalEdges = intraInternal[g] || 0;
    const totalEdges = groupTotalEdges[g] || 0;
    intraGroupDensity[g] = {
      internalEdges,
      totalEdges,
      density: totalEdges ? Number((internalEdges / totalEdges).toFixed(4)) : 0,
    };
  }

  const crossMap = {};
  for (const e of allEdges) {
    if (!e || !e.source || !e.target) continue;
    const s = nodeById.get(e.source);
    const t = nodeById.get(e.target);
    if (!s || !t) continue;
    const key = `${s.type || "file"}|${t.type || "file"}|${e.type || "related"}`;
    crossMap[key] = (crossMap[key] || 0) + 1;
  }
  const crossCategoryEdges = Object.entries(crossMap)
    .map(([k, count]) => {
      const [fromType, toType, edgeType] = k.split("|");
      return { fromType, toType, edgeType, count };
    })
    .sort((a, b) => b.count - a.count);

  const patternMatches = {};
  for (const g of Object.keys(directoryGroups)) {
    const dirLabel = matchDirPattern(g);
    if (dirLabel) {
      patternMatches[g] = dirLabel;
      continue;
    }
    const ids = directoryGroups[g];
    const votes = {};
    for (const id of ids) {
      const n = nodeById.get(id);
      const lab = filePatternLabel(n && n.filePath, n && n.type);
      if (lab) votes[lab] = (votes[lab] || 0) + 1;
    }
    let best = null, bestN = 0;
    for (const [lab, c] of Object.entries(votes)) {
      if (c > bestN) { best = lab; bestN = c; }
    }
    if (best && bestN >= Math.max(1, Math.ceil(ids.length * 0.4))) patternMatches[g] = best;
  }

  const pairDir = {};
  for (const row of interGroupImports) {
    const a = row.from, b = row.to;
    const key = a < b ? a + "\0" + b : b + "\0" + a;
    if (!pairDir[key]) pairDir[key] = { a: a < b ? a : b, b: a < b ? b : a, ab: 0, ba: 0 };
    if (row.from === pairDir[key].a) pairDir[key].ab += row.count;
    else pairDir[key].ba += row.count;
  }
  const dependencyDirection = [];
  for (const rec of Object.values(pairDir)) {
    if (rec.ab > rec.ba) dependencyDirection.push({ dependent: rec.a, dependsOn: rec.b });
    else if (rec.ba > rec.ab) dependencyDirection.push({ dependent: rec.b, dependsOn: rec.a });
  }

  const docGroups = new Set();
  const undocumentedGroups = [];
  for (const [g, ids] of Object.entries(directoryGroups)) {
    const hasDoc = ids.some((id) => {
      const n = nodeById.get(id);
      const p = normalizePath(n && n.filePath);
      return n && (n.type === "document" || /\.(md|rst)$/i.test(p) || basename(p).toLowerCase() === "readme.md");
    });
    if (hasDoc) docGroups.add(g);
    else undocumentedGroups.push(g);
  }
  const totalGroups = Object.keys(directoryGroups).length;
  const docCoverage = {
    groupsWithDocs: docGroups.size,
    totalGroups,
    coverageRatio: totalGroups ? Number((docGroups.size / totalGroups).toFixed(4)) : 0,
    undocumentedGroups,
  };

  const filesPerGroup = {};
  for (const [g, ids] of Object.entries(directoryGroups)) filesPerGroup[g] = ids.length;
  const nodeTypeCounts = {};
  for (const [t, ids] of Object.entries(nodeTypeGroups)) nodeTypeCounts[t] = ids.length;

  const result = {
    scriptCompleted: true,
    directoryGroups,
    nodeTypeGroups,
    crossCategoryEdges,
    interGroupImports,
    intraGroupDensity,
    patternMatches,
    deploymentTopology: detectDeployment(fileNodes),
    dataPipeline: detectDataPipeline(fileNodes),
    docCoverage,
    dependencyDirection,
    fileStats: {
      totalFileNodes: fileNodes.length,
      filesPerGroup,
      nodeTypeCounts,
    },
    fileFanIn,
    fileFanOut,
    commonPrefix: prefix,
    groupKeys: Object.keys(directoryGroups),
  };

  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, JSON.stringify(result, null, 2), "utf8");
}

try {
  main();
} catch (e) {
  die(e && e.stack ? e.stack : e);
}
