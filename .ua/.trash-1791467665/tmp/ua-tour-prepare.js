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

const uaDir = path.resolve(__dirname, "..");
const nodesIn = loadJson(path.join(uaDir, "tmp", "file-nodes.json"));
const edgesIn = loadJson(path.join(uaDir, "tmp", "all-edges.json"));
const layersIn = loadJson(path.join(uaDir, "intermediate", "layers.json"));

if (!Array.isArray(nodesIn)) die("file-nodes.json must be an array");
if (!Array.isArray(edgesIn)) die("all-edges.json must be an array");
if (!Array.isArray(layersIn)) die("layers.json must be an array");

const nodes = nodesIn.map((n) => ({
  id: n.id,
  name: n.name,
  filePath: n.filePath,
  summary: n.summary || "",
  type: n.type,
}));

const layers = layersIn.map((l) => ({
  id: l.id,
  name: l.name,
  description: l.description || "",
}));

const outPath = path.join(uaDir, "tmp", "ua-tour-input.json");
fs.writeFileSync(outPath, JSON.stringify({ nodes, edges: edgesIn, layers }));
process.stdout.write(`Wrote ${nodes.length} nodes, ${edgesIn.length} edges, ${layers.length} layers to ${outPath}\n`);
