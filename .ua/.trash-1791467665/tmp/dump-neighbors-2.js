const fs = require('fs');
const batches = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/intermediate/batches.json', 'utf8'));
const batch = batches.batches.find(b => b.batchIndex === 2);
const wanted = [
  'engine/editor/editor_undo.cpp',
  'engine/scene/scene_loader.cpp',
  'engine/ecs/prefab_serializer.cpp',
  'engine/ecs/tag_registry.cpp',
  'engine/ecs/tag_registry.h',
  'engine/editor/light_gizmo.cpp',
  'engine/scene/scene_lighting.cpp',
  'engine/scene/scene_runtime.cpp',
  'engine/ecs/material_component.cpp',
  'engine/ecs/directional_light_component.cpp'
];
for (const p of wanted) {
  const neighbors = (batch.neighborMap || {})[p] || [];
  console.log('\n====', p, 'neighbors:', neighbors.length);
  for (const n of neighbors) {
    console.log(' ', n.path, 'batch', n.batchIndex, 'symbols:', (n.symbols || []).slice(0, 20).join(', '), (n.symbols || []).length > 20 ? '...' : '');
  }
}
const exports = batches.exportsByPath || {};
for (const p of [
  'engine/editor/editor_undo.h',
  'engine/ecs/prefab_serializer.h',
  'engine/scene/scene_loader.h',
  'engine/ecs/game_object.h',
  'engine/scene/scene_lighting.h'
]) {
  console.log('\nexports', p, ':', (exports[p] || []).join(', '));
}
