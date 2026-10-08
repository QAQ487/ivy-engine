"use strict";
const fs = require("fs");
const path = "C:/Users/terry/source/repos/ivy-engine/.ua/intermediate/layers.json";
let s = fs.readFileSync(path, "utf8");
if (s.endsWith("]\\n")) {
  s = s.slice(0, -2) + "\n";
} else {
  const idx = s.lastIndexOf("]");
  if (idx < 0) throw new Error("no closing bracket");
  s = s.slice(0, idx + 1) + "\n";
}
fs.writeFileSync(path, s);
const layers = JSON.parse(fs.readFileSync(path, "utf8"));
const nodes = JSON.parse(fs.readFileSync("C:/Users/terry/source/repos/ivy-engine/.ua/tmp/file-nodes.json", "utf8"));
const inputIds = new Set(nodes.map((n) => n.id));
const seen = new Set();
let ok = true;
for (const L of layers) {
  if (!L.id || !L.name || !L.description || !Array.isArray(L.nodeIds) || !L.nodeIds.length) ok = false;
  for (const id of L.nodeIds) {
    if (!inputIds.has(id) || seen.has(id)) ok = false;
    seen.add(id);
  }
}
const missing = nodes.filter((n) => !seen.has(n.id));
console.log("valid", ok && missing.length === 0 && seen.size === nodes.length);
console.log("layers", layers.length, "assigned", seen.size, "input", nodes.length, "missing", missing.length);
for (const L of layers) console.log(L.id, L.nodeIds.length);
