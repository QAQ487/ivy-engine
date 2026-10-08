import fs from "fs";

const d = JSON.parse(
  fs.readFileSync(
    "C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-extract-results-3.json",
    "utf8"
  )
);

for (const r of d.results) {
  console.log("\n====", r.path, "====");
  console.log(
    `lang=${r.language} cat=${r.fileCategory} lines=${r.totalLines} nonEmpty=${r.nonEmptyLines}`
  );
  console.log("metrics:", JSON.stringify(r.metrics));
  if (r.functions?.length) {
    console.log("functions:");
    for (const f of r.functions) {
      const span = f.endLine - f.startLine + 1;
      console.log(
        `  ${f.name} L${f.startLine}-${f.endLine} (${span} lines) params=${JSON.stringify(f.params || [])}`
      );
    }
  }
  if (r.classes?.length) {
    console.log("classes:");
    for (const c of r.classes) {
      const span = c.endLine - c.startLine + 1;
      console.log(
        `  ${c.name} L${c.startLine}-${c.endLine} (${span} lines) methods=${JSON.stringify(c.methods || [])} props=${JSON.stringify(c.properties || [])}`
      );
    }
  }
  if (r.exports?.length) {
    console.log(
      "exports:",
      r.exports.map((e) => `${e.name}@${e.line}${e.isDefault ? " default" : ""}`).join(", ")
    );
  }
  if (r.callGraph?.length) {
    console.log("callGraph:");
    for (const c of r.callGraph) {
      console.log(`  ${c.caller} -> ${c.callee} @${c.lineNumber}`);
    }
  }
}
