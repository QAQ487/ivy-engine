import fs from "fs";

const uaDir = "C:/Users/terry/source/repos/ivy-engine/.ua";
const batches = JSON.parse(
  fs.readFileSync(`${uaDir}/intermediate/batches.json`, "utf8")
);
const batch = batches.batches.find((b) => b.batchIndex === 3);
if (!batch) {
  console.error("batch 3 not found");
  process.exit(1);
}
const input = {
  projectRoot: "C:/Users/terry/source/repos/ivy-engine",
  batchFiles: batch.files.map((f) => ({
    path: f.path,
    language: f.language,
    sizeLines: f.sizeLines,
    fileCategory: f.fileCategory,
  })),
  batchImportData: batch.batchImportData,
};
fs.writeFileSync(
  `${uaDir}/tmp/ua-file-analyzer-input-3.json`,
  JSON.stringify(input, null, 2)
);
const importCount = Object.values(input.batchImportData).reduce(
  (n, a) => n + a.length,
  0
);
fs.writeFileSync(
  `${uaDir}/tmp/ua-file-analyzer-neighbor-3.json`,
  JSON.stringify(batch.neighborMap || {}, null, 2)
);
console.log("files:", input.batchFiles.length);
console.log("import keys:", Object.keys(input.batchImportData).length);
console.log("neighborMap keys:", Object.keys(batch.neighborMap || {}).length);
console.log("total imports:", importCount);
console.log("files list:");
for (const f of input.batchFiles) {
  console.log(`  ${f.path} (${f.language}, ${f.sizeLines}, ${f.fileCategory})`);
}
