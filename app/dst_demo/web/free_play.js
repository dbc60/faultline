// Mounts a free-play demo on each element with a data-dst-demo attribute:
//
//   <div data-dst-demo data-wasm="dst_demo.wasm" data-trace="large_bins.json"></div>
//
// With data-trace, the demo plays the trace first. Its controls open when the reader
// continues, on the tree the trace ended with.

import { arenaSteps, traceTitle } from './arena_steps.js';
import { DstWasm } from './dst_wasm.js';
import { Player } from './player.js';
import { StepLog } from './step_log.js';
import { addSteps, findSteps, removeSteps } from './steps.js';
import { TreeView } from './tree_view.js';

const KEY_MIN = 1024;
const KEY_STEP = 16;
const KEY_SLOTS = 32;
const KEY_MAX = KEY_MIN + (KEY_SLOTS - 1) * KEY_STEP;
const RANDOM_NODES = 12;

// With these keys, finding 1168 reaches the saved subtree: the search dead-ends at 1024's
// left side, and the best fit is 1296 under the saved right child.
const OPENING_KEYS = [1024, 1488, 1296, 1424];
const OPENING_FIND = 1168;

const SPEEDS = [
    ['Slow', 1800],
    ['Normal', 900],
    ['Fast', 350],
];
const DEFAULT_SPEED = 900;

const LEGEND = [
    ['visit', 'Visiting'],
    ['best', 'Best fit so far'],
    ['saved', 'Saved subtree'],
    ['smallest', 'Smallest in subtree'],
    ['target', 'Removing'],
    ['replacement', 'Replacement'],
    ['result', 'Result'],
];

const TEMPLATE = `
    <h3 class="title" hidden></h3>
    <form class="controls">
        <label>Size <input name="size" type="number" min="${KEY_MIN}" max="${KEY_MAX}"
            step="${KEY_STEP}" value="${OPENING_FIND}" required></label>
        <button name="add" type="submit">Add</button>
        <button name="find" type="button">Find</button>
        <button name="remove" type="button">Remove</button>
        <button name="random" type="button">Random tree</button>
        <button name="clear" type="button">Clear</button>
    </form>
    <button class="continue" name="continue" type="button" hidden>Continue in free play</button>
    <div class="stage">
        <div class="tree"></div>
        <div class="sidebar">
            <ol class="log" role="log" aria-label="Steps"></ol>
        </div>
    </div>
    <div class="transport">
        <button name="previous" type="button" aria-label="Previous step">‹</button>
        <button name="play" type="button">Pause</button>
        <button name="next" type="button" aria-label="Next step">›</button>
        <span class="position"></span>
        <label>Speed <select name="speed"></select></label>
    </div>
    <ul class="legend"></ul>
    <p class="hint">Sizes are multiples of ${KEY_STEP} from ${KEY_MIN} to ${KEY_MAX}.
        Select a node to remove it. ×N counts same-size nodes.</p>
`;

const validKey = (key) =>
    Number.isInteger(key) && key >= KEY_MIN && key <= KEY_MAX && (key - KEY_MIN) % KEY_STEP === 0;

function randomKey() {
    return KEY_MIN + Math.floor(Math.random() * KEY_SLOTS) * KEY_STEP;
}

class FreePlay {
    constructor(root, wasm, trace) {
        this.root = root;
        this.wasm = wasm;
        this.scripted = false;

        root.classList.add('dst-demo');
        root.innerHTML = TEMPLATE;

        this.form = root.querySelector('form');
        this.position = root.querySelector('.position');
        this.playButton = root.querySelector('[name=play]');

        const tree = new TreeView(root.querySelector('.tree'), (id) => this.select(id));
        const log = new StepLog(root.querySelector('.log'));

        // The player sets its index before it renders, so the log can read it.
        const captions = { render: () => log.render(this.player.steps, this.player.index) };

        this.player = new Player([tree, captions], () => this.updateTransport());
        this.buildSpeeds();
        this.buildLegend();
        this.connect();

        if (trace !== null) {
            this.playTrace(trace);
        } else {
            this.fill(OPENING_KEYS);
            this.find(OPENING_FIND);
        }
    }

    playTrace(trace) {
        const title = this.root.querySelector('.title');

        title.textContent = traceTitle(trace);
        title.hidden = false;
        this.steps = arenaSteps(trace);
        this.setScripted(true);
        this.player.load(this.steps);
    }

    setScripted(scripted) {
        this.scripted = scripted;
        this.root.querySelector('[name=continue]').hidden = !scripted;
        this.form.hidden = scripted;
        this.root.querySelector('.hint').hidden = scripted;
    }

    // The trace's chunks become demo nodes, so the tree keeps its shape under new ids.
    continueFreePlay() {
        this.wasm.restore(this.steps.at(-1).tree);
        this.setScripted(false);
        this.notice('This tree now runs in your browser. Add, remove or find a size.');
    }

    select(id) {
        if (!this.scripted) {
            this.remove(id);
        }
    }

    buildSpeeds() {
        const select = this.root.querySelector('[name=speed]');

        for (const [label, delay] of SPEEDS) {
            select.add(new Option(label, delay, false, delay === DEFAULT_SPEED));
        }

        select.addEventListener('change', () => (this.player.delay = Number(select.value)));
    }

    buildLegend() {
        const list = this.root.querySelector('.legend');

        for (const [role, label] of LEGEND) {
            const item = document.createElement('li');
            item.className = role;
            item.textContent = label;
            list.append(item);
        }
    }

    connect() {
        const button = (name, action) =>
            this.root.querySelector(`[name=${name}]`).addEventListener('click', action);

        this.form.addEventListener('submit', (event) => {
            event.preventDefault();
            this.withKey((key) => this.add(key));
        });

        button('find', () => this.withKey((key) => this.find(key)));
        button('remove', () => this.withKey((key) => this.removeKey(key)));
        button('random', () => this.randomTree());
        button('clear', () => this.clear());
        button('continue', () => this.continueFreePlay());
        button('previous', () => this.player.previous());
        button('next', () => this.player.next());
        button('play', () => (this.player.running ? this.player.pause() : this.player.play()));
    }

    withKey(action) {
        const key = this.form.elements.size.valueAsNumber;

        if (validKey(key)) {
            action(key);
        } else {
            this.notice(`Enter a multiple of ${KEY_STEP} from ${KEY_MIN} to ${KEY_MAX}.`);
        }
    }

    notice(caption) {
        this.player.load([{ tree: this.wasm.snapshot(), marks: {}, ghost: null, caption }]);
    }

    add(key) {
        const before = this.wasm.snapshot();
        const { id, frames } = this.wasm.add(key);

        if (id === null) {
            this.notice(`The demo holds ${this.wasm.capacity} nodes, and all are in use.`);
            return;
        }

        this.player.load(addSteps(before, this.wasm.snapshot(), frames, id, key));
    }

    find(key) {
        const tree = this.wasm.snapshot();
        const { id, frames } = this.wasm.find(key);

        if (tree.nodes.length === 0) {
            this.notice('The tree is empty.');
            return;
        }

        this.player.load(findSteps(tree, frames, id, key));
    }

    remove(id) {
        const before = this.wasm.snapshot();
        const { id: removed, frames } = this.wasm.remove(id);

        // The view can show an earlier step, with a node that has since left the tree.
        if (removed === null) {
            this.notice('That node has already been removed.');
            return;
        }

        this.player.load(removeSteps(before, this.wasm.snapshot(), frames, id));
    }

    removeKey(key) {
        const node = this.wasm.snapshot().nodes.find((candidate) => candidate.key === key);

        if (node === undefined) {
            this.notice(`The tree holds no node of ${key} bytes.`);
            return;
        }

        this.remove(node.id);
    }

    fill(keys) {
        this.wasm.reset();
        keys.forEach((key) => this.wasm.add(key));
    }

    randomTree() {
        this.fill(Array.from({ length: RANDOM_NODES }, randomKey));
        this.notice(`A tree of ${RANDOM_NODES} random sizes.`);
    }

    clear() {
        this.wasm.reset();
        this.notice('The tree is empty.');
    }

    updateTransport() {
        const { player } = this;

        this.playButton.textContent = player.running ? 'Pause' : player.atEnd ? 'Replay' : 'Play';
        this.position.textContent = `Step ${player.index + 1} of ${player.steps.length}`;
    }
}

async function loadTrace(url) {
    if (url === undefined) {
        return null;
    }

    const response = await fetch(url);

    if (!response.ok) {
        throw new Error(`${url}: HTTP ${response.status}`);
    }

    return response.json();
}

async function mount(root) {
    const [wasm, trace] = await Promise.all([DstWasm.load(root.dataset.wasm), loadTrace(root.dataset.trace)]);

    return new FreePlay(root, wasm, trace);
}

for (const root of document.querySelectorAll('[data-dst-demo]')) {
    mount(root).catch((error) => {
        root.textContent = `The demo failed to load: ${error.message}`;
    });
}
