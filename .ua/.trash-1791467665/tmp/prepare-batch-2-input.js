const fs = require('fs');
const batches = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/intermediate/batches.json', 'utf8'));
const batch = batches.batches.find(b => b.batchIndex === 2);
if (!batch) { console.error('batch 2 not found'); process.exit(1); }
const input = {
  projectRoot: 'C:/Users/terry/source/repos/ivy-engine',
  batchFiles: batch.files.map(f => ({
    path: f.path,
    language: f.language,
    sizeLines: f.sizeLines,
    fileCategory: f.fileCategory
  })),
  batchImportData: batch.batchImportData
};
fs.writeFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-analyzer-input-2.json', JSON.stringify(input, null, 2));
console.log('files:', input.batchFiles.length);
console.log('import keys:', Object.keys(input.batchImportData).length);
const importSum = Object.values(input.batchImportData).reduce((s, a) => s + a.length, 0);
console.log('import edge expected:', importSum);
console.log('neighborMap keys:', Object.keys(batch.neighborMap || {}).length);
if (batch.previousSymbols) console.log('previousSymbols:', batch.previousSymbols.length);
else console.log('previousSymbols: none');
