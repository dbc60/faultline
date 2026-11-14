// Checks the page's modules that need no browser, against the real dst_demo.wasm: the
// wasm wrapper, step building, tree layout and the player.
//
// Usage: node dst_demo_web_check.mjs path\to\dst_demo.wasm path\to\trace.json...
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';

import { arenaSteps } from './web/arena_steps.js';
import { DstWasm } from './web/dst_wasm.js';
import { Player } from './web/player.js';
import { addSteps, findSteps, removeSteps } from './web/steps.js';
import { HEIGHT, NODE_WIDTH, WIDTH, layoutTree } from './web/tree_layout.js';

const KEY_MIN = 1024;
const KEY_STEP = 16;
const KEY_SLOTS = 32;
const OPERATIONS = 20000;

const ADD_PERCENT = 45;
const REMOVE_PERCENT = 35; // the remainder are finds

const wasm = await DstWasm.fromBytes(readFileSync(process.argv[2]));

// Linear congruential generator, so a failure repeats.
let seed = 54321;
function random(limit) {
    seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
    return (seed >>> 8) % limit;
}

const randomKey = () => KEY_MIN + random(KEY_SLOTS) * KEY_STEP;

function allIds(tree) {
    return tree.nodes.flatMap((node) => [node.id, ...node.stack]);
}

// Tree nodes sit inside the drawing, apart from each other, on real edges.
function assertLayout(tree) {
    const { boxes, edges } = layoutTree(tree);
    const placed = [...boxes.values()].filter((box) => box.role === 'tree');

    assert.equal(boxes.size, allIds(tree).length, 'every node has a box');
    assert.equal(edges.length, Math.max(tree.nodes.length - 1, 0), 'one edge per non-root node');

    for (const box of boxes.values()) {
        assert.ok(box.x > 0 && box.x < WIDTH, 'x is inside the drawing');
        assert.ok(box.y > 0 && box.y < HEIGHT, 'y is inside the drawing');
    }

    placed.forEach((box, index) => {
        for (const other of placed.slice(index + 1)) {
            const apart = box.y !== other.y || Math.abs(box.x - other.x) >= NODE_WIDTH;
            assert.ok(apart, `${box.key} overlaps ${other.key}`);
        }
    });

    for (const edge of edges) {
        assert.ok(boxes.has(edge.from) && boxes.has(edge.to), 'an edge joins two nodes');
    }
}

function assertSteps(steps, after) {
    assert.ok(steps.length > 0);

    for (const step of steps) {
        assert.ok(step.caption.length > 0, 'each step has a caption');
        assert.ok(!/undefined|null|NaN/.test(step.caption), step.caption);

        const ids = allIds(step.tree);
        for (const [role, id] of Object.entries(step.marks)) {
            assert.ok(ids.includes(id), `mark ${role} names a node in the step's tree`);
        }
        if (step.ghost !== null) {
            assert.ok(ids.includes(step.ghost.near), 'a ghost sits beside a node in the tree');
        }
    }

    assert.deepEqual(steps.at(-1).tree, after, 'the last step shows the resulting tree');
}

function checkRandomOperations() {
    let live = [];
    wasm.reset();

    for (let i = 0; i < OPERATIONS; i++) {
        const before = wasm.snapshot();
        const roll = random(100);
        let steps = null;

        if (roll < ADD_PERCENT) {
            const key = randomKey();
            const { id, frames } = wasm.add(key);
            if (id !== null) {
                live.push(id);
                steps = addSteps(before, wasm.snapshot(), frames, id, key);
            }
        } else if (roll < ADD_PERCENT + REMOVE_PERCENT && live.length > 0) {
            const id = live[random(live.length)];
            const { frames } = wasm.remove(id);
            live = live.filter((other) => other !== id);
            steps = removeSteps(before, wasm.snapshot(), frames, id);
        } else if (before.nodes.length > 0) {
            const key = randomKey();
            const { id, frames } = wasm.find(key);
            steps = findSteps(before, frames, id, key);
        }

        const after = wasm.snapshot();
        assertLayout(after);
        if (steps !== null) {
            assertSteps(steps, after);
        }
    }
}

// The tree that reaches the saved subtree must end on the subtree's smallest key.
function checkSavedSubtreeCaption() {
    wasm.reset();
    [1024, 1488, 1296, 1424].forEach((key) => wasm.add(key));

    const tree = wasm.snapshot();
    const { id, frames } = wasm.find(1168);
    const steps = findSteps(tree, frames, id, 1168);

    assert.equal(steps.at(-1).caption, 'Best fit for 1168 is 1296, with 128 bytes left over.');
    assert.ok(steps.some((step) => step.marks.saved !== undefined), 'a step marks the saved subtree');

    const deadEnd = steps.find((step) => step.caption.includes('bit-guided search ends'));
    assert.equal(deadEnd.marks.visit, frames.find((frame) => frame.event === 'find_dead_end').node);
}

function checkSiblingPromotion() {
    wasm.reset();
    const first = wasm.add(1280).id;
    wasm.add(1280);

    const before = wasm.snapshot();
    const { frames } = wasm.remove(first);
    const steps = removeSteps(before, wasm.snapshot(), frames, first);

    assert.equal(steps.at(-1).tree.nodes.length, 1);
    assert.deepEqual(steps.at(-1).tree.nodes[0].stack, []);
    assert.equal(steps.at(-1).marks.result, steps.at(-1).tree.nodes[0].id);
    assert.match(steps.at(-1).caption, /sibling now holds/);
}

const shape = (tree) => tree.nodes.map((node) => [node.key, node.path, node.stack.length]);

// The trace covers each kind of tree step, and free play can take over its last tree.
function checkTrace(path) {
    const trace = JSON.parse(readFileSync(path));
    const steps = arenaSteps(trace);

    assertSteps(steps, steps.at(-1).tree);
    steps.forEach((step) => assertLayout(step.tree));

    const captions = steps.map((step) => step.caption).join('\n');
    for (const expected of ['attaches there', 'sibling list', 'takes over', 'leftmost leaf', 'saved subtree', 'free neighbor']) {
        assert.ok(captions.includes(expected), `no step says: ${expected}`);
    }

    wasm.restore(steps.at(-1).tree);
    assert.deepEqual(shape(wasm.snapshot()), shape(steps.at(-1).tree), 'free play keeps the shape');

    return steps;
}

function checkPlayer() {
    const shown = [];
    const view = { render: (step) => shown.push(step) };
    const runningAtChange = [];
    const player = new Player([view], () => runningAtChange.push(player.running));

    player.load(['x', 'y']);
    assert.equal(runningAtChange.at(-1), true, 'the last change after load reports running');
    player.pause();

    player.load(['a', 'b', 'c']);
    assert.equal(player.running, false, 'a pause lasts across load');
    shown.length = 0;
    shown.push('a');
    player.next();
    player.next();
    player.next();
    assert.equal(player.index, 2, 'next stops at the last step');
    player.previous();
    assert.deepEqual(shown, ['a', 'b', 'c', 'c', 'b']);

    player.load(['d']);
    player.play();
    assert.equal(player.running, false, 'a single step has nothing to schedule');
    player.load(['e', 'f']);
    assert.equal(player.running, true);
    player.pause();
    assert.equal(player.running, false);
}

checkRandomOperations();
checkSavedSubtreeCaption();
checkSiblingPromotion();
checkPlayer();
console.log(`dst_demo web: ${OPERATIONS} operations through steps and layout passed`);

for (const path of process.argv.slice(3)) {
    const steps = checkTrace(path);
    console.log(`dst_demo web: ${path}: ${steps.length} steps passed`);
}
