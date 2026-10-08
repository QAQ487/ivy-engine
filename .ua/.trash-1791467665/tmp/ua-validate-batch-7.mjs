import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

const dir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
const files = readdirSync(dir)
  .filter((f) => /^batch-7(?:-part-\d+)?\.json$/.test(f))
  .sort((a, b) => {
    const na = parseInt((a.match(/part-(\d+)/) || [, '0'])[1], 10);
    const nb = parseInt((b.match(/part-(\d+)/) || [, '0'])[1], 10);
    return na - nb;
  });

const input = JSON.parse(readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-input-7.json', 'utf8'));
const neighborMap = JSON.parse(readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-neighbor-7.json', 'utf8'));
const batchFiles = new Set(input.batchFiles.map((f) => f.path));
const importTargets = new Set(Object.values(input.batchImportData).flat());
const neighborFiles = new Set();
const neighborSymbols = new Map();
for (const arr of Object.values(neighborMap)) {
  for (const n of arr) {
    neighborFiles.add(n.path);
    const s = neighborSymbols.get(n.path) || new Set();
    for (const x of n.symbols || []) s.add(x);
    neighborSymbols.set(n.path, s);
  }
}

const issues = [];
const seenIds = new Set();
const allNodes = [];
const allEdges = [];

function targetOk(target, partNodeIds) {
  if (partNodeIds.has(target)) return true;
  if (target.startsWith('file:')) {
    const p = target.slice(5);
    return neighborFiles.has(p) || importTargets.has(p) || batchFiles.has(p);
  }
  const m = target.match(/^(?:function|class):(.+):(.+)$/);
  if (!m) return false;
  const [, p, symbol] = m;
  const syms = neighborSymbols.get(p);
  return !!(syms && syms.has(symbol));
}

for (const f of files) {
  const j = JSON.parse(readFileSync(join(dir, f), 'utf8'));
  if (!Array.isArray(j.nodes) || !Array.isArray(j.edges)) issues.push(`${f} missing arrays`);
  const partIds = new Set(j.nodes.map((n) => n.id));
  for (const n of j.nodes) {
    if (seenIds.has(n.id)) issues.push(`dup id ${n.id}`);
    seenIds.add(n.id);
    if (!n.summary) issues.push(`empty summary ${n.id}`);
    if (!n.tags || n.tags.length < 3 || n.tags.length > 5) issues.push(`tags ${n.id}`);
    if (!['simple', 'moderate', 'complex'].includes(n.complexity)) issues.push(`complexity ${n.id}`);
    if (!n.name) issues.push(`name ${n.id}`);
  }
  for (const e of j.edges) {
    if (e.source === e.target) issues.push(`self ${e.source}`);
    if (!partIds.has(e.source)) issues.push(`${f} source not in part ${e.source}`);
    if (!targetOk(e.target, partIds)) issues.push(`${f} bad target ${e.type} ${e.target}`);
    if (e.direction !== 'forward') issues.push(`dir ${e.source}`);
  }
  allNodes.push(...j.nodes);
  allEdges.push(...j.edges);
  console.log(f, 'nodes', j.nodes.length, 'edges', j.edges.length);
}

for (const bf of batchFiles) {
  if (!allNodes.some((n) => n.id === `file:${bf}`)) issues.push(`missing file node ${bf}`);
}
const imports = allEdges.filter((e) => e.type === 'imports');
if (imports.length !== 60) issues.push(`imports ${imports.length} expected 60`);

console.log('parts', files.length);
console.log('nodes', allNodes.length, 'edges', allEdges.length);
console.log('files', allNodes.filter((n) => n.type === 'file').length);
console.log('functions', allNodes.filter((n) => n.type === 'function').length);
console.log('classes', allNodes.filter((n) => n.type === 'class').length);
console.log('issues', issues.length);
for (const i of issues.slice(0, 40)) console.log(' ISSUE', i);
if (issues.length) process.exit(1);
