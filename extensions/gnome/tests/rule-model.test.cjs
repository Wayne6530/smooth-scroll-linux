const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(__dirname + '/../smooth-scroll@wayne6530/rules.js', 'utf8');
const context = vm.createContext({});
vm.runInContext(source.replace(/^export /gm, '') + '\nglobalThis.api = {titleMatcher, normalizeRules, matchWindow, rulesFromVariant, rulesToVariant};', context);
const {titleMatcher, normalizeRules, matchWindow, rulesFromVariant, rulesToVariant} = context.api;
const condition = (mode, title, extra = {}) => ({enabled: true, mode, title, ignore_case: false, profile_id: 3, ...extra});
assert.equal(titleMatcher(condition('contains', 'a.b (c)'))('aXb c'), false);
assert.equal(titleMatcher(condition('contains', 'a.b (c)'))('prefix a.b (c) suffix'), true);
assert.equal(titleMatcher(condition('equals', 'Docs'))('My Docs'), false);
assert.equal(titleMatcher(condition('equals', 'Docs', {ignore_case: true}))('DOCS'), true);
assert.equal(titleMatcher(condition('starts-with', 'Docs'))('Docs - Browser'), true);
assert.equal(titleMatcher(condition('ends-with', 'Docs'))('My Docs'), true);
assert.equal(titleMatcher(condition('contains', 'Docs'))('docs'), false);
assert.equal(titleMatcher(condition('regex', '^docs$', {ignore_case: true}))('Docs'), true);
assert.throws(() => titleMatcher(condition('regex', '[')));
assert.throws(() => titleMatcher(condition('contains', '')));
assert.throws(() => titleMatcher(condition('unknown', 'Docs')));
const rule = {app: 'browser.desktop', name: 'Browser', icon: 'browser', enabled: true, profile_id: 2, titles: [
  condition('contains', 'Docs', {enabled: false, profile_id: 9}),
  condition('contains', 'Docs'),
  condition('contains', 'Docs', {profile_id: 4}),
]};
const evaluate = (rules, app = 'browser.desktop', title = 'Docs - Browser') => matchWindow(normalizeRules(rules), {appIds: [app], title}).profileId;
assert.equal(evaluate([rule]), 3);
assert.equal(evaluate([rule], 'browser.desktop', 'Home'), 2);
assert.equal(evaluate([rule], 'other.desktop'), 0);
assert.equal(evaluate([{...rule, enabled: false}]), 0);
assert.equal(evaluate([{...rule, titles: [condition('regex', '[')]}]), 2);
const roundTrip = rulesFromVariant(rulesToVariant([rule]));
assert.equal(JSON.stringify(roundTrip), JSON.stringify([rule]));
assert.throws(() => rulesToVariant([rule, rule]));
for (const profile_id of [-1, 0.5, 0x100000000, NaN])
  assert.throws(() => rulesToVariant([{...rule, profile_id}]));
assert.equal(evaluate([{...rule, profile_id: 0xffffffff, titles: []}]), 0xffffffff);
console.log('Literal/regex matching, precedence, enabled states, uint32 IDs and structured rule round-trips passed.');
