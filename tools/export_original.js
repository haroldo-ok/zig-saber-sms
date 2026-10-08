// Export deterministic upstream courses, generated art and sound definitions.
// The upstream snapshot and its MIT licence are shipped alongside this tool.
const fs = require('fs'), vm = require('vm');
const ZS = require('../upstream/core.js'), S = require('../upstream/psg.js');
const main = fs.readFileSync(require.resolve('../upstream/main.js'), 'utf8');
function object(name) {
  const text = main.match(new RegExp('const ' + name + ' = (\\{[\\s\\S]*?\\n  \\});'))[1];
  return vm.runInNewContext('(' + text + ')');
}
const effects = [];
for (const [name, mk] of Object.entries(S.PROGRAMS)) {
  const args = name === 'cut' ? [0,1,2,3,4] : name === 'gem' ? [1,2,3,4,5,6,7,8] : [0];
  for (const arg of args) effects.push({ name, arg, ...mk(arg) });
}
const music = Array.from({length:4}, (_,zone) => Array.from({length:zone === 3 ? 64 : 128}, (_,i) => S.stepEvents(i,zone,0,false)));
fs.writeFileSync('assets/original.json', JSON.stringify({courses:[ZS.buildCourse(0),ZS.buildCourse(1)],font:object('FONT'),sprites:object('SPR'),music,effects}));
console.log('Exported the original deterministic courses, font, sprites and PSG programs.');
