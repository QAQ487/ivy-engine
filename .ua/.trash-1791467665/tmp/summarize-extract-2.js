const fs = require('fs');
const data = JSON.parse(fs.readFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-extract-results-2.json', 'utf8'));
const summary = data.results.map(r => ({
  path: r.path,
  language: r.language,
  totalLines: r.totalLines,
  nonEmptyLines: r.nonEmptyLines,
  functions: (r.functions || []).map(f => ({
    name: f.name,
    startLine: f.startLine,
    endLine: f.endLine,
    lines: (f.endLine - f.startLine + 1),
    params: f.params
  })),
  classes: (r.classes || []).map(c => ({
    name: c.name,
    startLine: c.startLine,
    endLine: c.endLine,
    lines: (c.endLine - c.startLine + 1),
    methods: c.methods,
    properties: c.properties
  })),
  exports: (r.exports || []).map(e => e.name),
  callGraph: (r.callGraph || []).slice(0, 80).map(c => `${c.caller} -> ${c.callee} @${c.lineNumber}`),
  callGraphCount: (r.callGraph || []).length,
  metrics: r.metrics
}));
fs.writeFileSync('C:/Users/terry/source/repos/ivy-engine/.ua/tmp/ua-file-extract-summary-2.json', JSON.stringify({
  scriptCompleted: data.scriptCompleted,
  filesAnalyzed: data.filesAnalyzed,
  filesSkipped: data.filesSkipped,
  filesUnreadable: data.filesUnreadable,
  summary
}, null, 2));
console.log('summarized', summary.length, 'files');
summary.forEach(s => {
  console.log(`\n=== ${s.path} (${s.nonEmptyLines} nonempty) ===`);
  console.log('  classes:', s.classes.map(c => `${c.name}[${c.startLine}-${c.endLine}] methods=${c.methods.join(',')}`).join(' | ') || '-');
  console.log('  functions:', s.functions.map(f => `${f.name}[${f.startLine}-${f.endLine} L${f.lines}]`).join(', ') || '-');
  console.log('  exports:', s.exports.join(', ') || '-');
  console.log('  calls:', s.callGraphCount);
});
