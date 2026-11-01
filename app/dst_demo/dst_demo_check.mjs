// Checks dst_demo.wasm in Node: known cases, then random operations with the tree's
// invariants verified after each one.
//
// Usage: node dst_demo_check.mjs path\to\dst_demo.wasm
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';

const NO_NODE = -1;
const KEY_MIN = 1024;
const KEY_STEP = 16;
const KEY_SLOTS = 32;
const FIRST_DECISION_BIT = 8;
const OPERATIONS = 200000;

const ADD_PERCENT = 45;
const REMOVE_PERCENT = 35; // the remainder are finds

const wasm = await WebAssembly.compile(readFileSync(process.argv[2]));
assert.deepEqual(WebAssembly.Module.imports(wasm), [], 'a browser must load it with no imports');
const { exports: demo } = await WebAssembly.instantiate(wasm);

// Linear congruential generator, so a failure repeats.
let seed = 12345;
function random(limit) {
    seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
    return (seed >>> 8) % limit;
}

const randomKey = () => KEY_MIN + random(KEY_SLOTS) * KEY_STEP;

function bestFit(keys, key) {
    let best = NO_NODE;
    for (const candidate of keys) {
        if (candidate >= key && (best === NO_NODE || candidate < best)) {
            best = candidate;
        }
    }
    return best;
}

// Walks one node, its sibling ring and both subtrees, and collects every id reached.
function walk(id, depth, prefix, reached) {
    if (id === NO_NODE) {
        return;
    }

    assert.ok(!reached.has(id), `node ${id} reached twice`);
    reached.add(id);

    const key = demo.key(id);
    const bit = FIRST_DECISION_BIT - depth + 1;
    assert.equal((key - KEY_MIN) >> bit, prefix, `key ${key} is off its path`);

    for (let next = demo.next_sibling(id); next !== NO_NODE && next !== id; next = demo.next_sibling(next)) {
        assert.ok(!reached.has(next), `sibling ${next} reached twice`);
        reached.add(next);
        assert.equal(demo.key(next), key, 'siblings share a key');
        assert.equal(demo.left(next), NO_NODE, 'a sibling has no left child');
        assert.equal(demo.right(next), NO_NODE, 'a sibling has no right child');
    }

    walk(demo.left(id), depth + 1, prefix << 1, reached);
    walk(demo.right(id), depth + 1, (prefix << 1) | 1, reached);
}

function assertTreeHolds(model) {
    const reached = new Set();
    walk(demo.root(), 0, 0, reached);
    assert.deepEqual([...reached].sort((a, b) => a - b), [...model.keys()].sort((a, b) => a - b));
}

// Same tree as "Find Best Fit In Saved Subtree" in digital_search_tree_tests.c.
function checkKnownCases() {
    demo.reset();
    assert.equal(demo.root(), NO_NODE);
    assert.equal(demo.find(1168), NO_NODE, 'an empty tree has no fit');

    const ids = [1024, 1488, 1296, 1424].map((key) => demo.add(key));
    assert.equal(demo.root(), ids[0]);
    assert.equal(demo.key(demo.find(1168)), 1296, 'best fit is in the saved subtree');
    assert.equal(demo.find(1504), NO_NODE, 'no key is large enough');

    assert.equal(demo.add(1000), NO_NODE, 'below the bin');
    assert.equal(demo.add(1536), NO_NODE, 'above the bin');
    assert.equal(demo.add(1028), NO_NODE, 'not on a chunk boundary');
    assert.equal(demo.remove(NO_NODE), NO_NODE, 'unknown id');

    demo.reset();
    const capacity = demo.capacity();
    for (let i = 0; i < capacity; i++) {
        assert.notEqual(demo.add(KEY_MIN), NO_NODE);
    }
    assert.equal(demo.add(KEY_MIN), NO_NODE, 'pool is full');

    // Removing the root of a same-size stack promotes a sibling.
    assert.equal(demo.remove(demo.root()), 0);
    assert.notEqual(demo.root(), NO_NODE);
}

function checkRandomOperations() {
    const model = new Map(); // id -> key
    const counts = { add: 0, remove: 0, find: 0 };
    demo.reset();

    for (let i = 0; i < OPERATIONS; i++) {
        const roll = random(100);

        if (roll < ADD_PERCENT) {
            const key = randomKey();
            const id = demo.add(key);
            if (id !== NO_NODE) {
                model.set(id, key);
                counts.add++;
            }
        } else if (roll < ADD_PERCENT + REMOVE_PERCENT && model.size > 0) {
            const id = [...model.keys()][random(model.size)];
            assert.equal(demo.remove(id), id);
            model.delete(id);
            counts.remove++;
        } else {
            const key = randomKey();
            const found = demo.find(key);
            const expected = bestFit(model.values(), key);
            assert.equal(found === NO_NODE ? NO_NODE : demo.key(found), expected, `find ${key}`);
            counts.find++;
        }

        assertTreeHolds(model);
    }

    return counts;
}

checkKnownCases();
const counts = checkRandomOperations();
console.log(`dst_demo: ${readFileSync(process.argv[2]).length} bytes, no imports`);
console.log(`dst_demo: ${counts.add} adds, ${counts.remove} removes, ${counts.find} finds passed`);
