// The only module that touches dst_demo.wasm. Callers work with snapshots and named
// frames rather than raw exports.

const NO_NODE = -1;

// Fails on a server that sends the wrong MIME type, so fall back to the bytes.
async function instantiate(url) {
    try {
        return await WebAssembly.instantiateStreaming(fetch(url));
    } catch (streamingError) {
        const response = await fetch(url);
        if (!response.ok) {
            throw streamingError;
        }
        return WebAssembly.instantiate(await response.arrayBuffer());
    }
}

export class DstWasm {
    static async load(url) {
        const { instance } = await instantiate(url);
        return new DstWasm(instance.exports);
    }

    static async fromBytes(bytes) {
        const { instance } = await WebAssembly.instantiate(bytes);
        return new DstWasm(instance.exports);
    }

    constructor(exports) {
        this.demo = exports;
        this.eventNames = Array.from({ length: exports.event_count() }, (_, event) =>
            this.readName(exports.event_name(event)),
        );
    }

    readName(address) {
        const memory = new Uint8Array(this.demo.memory.buffer);
        const end = memory.indexOf(0, address);
        return new TextDecoder().decode(memory.subarray(address, end));
    }

    reset() {
        this.demo.reset();
    }

    get capacity() {
        return this.demo.capacity();
    }

    // Rebuilds a snapshot's tree under new ids. A parent precedes its children in a
    // snapshot, so each node attaches at its original path.
    restore(tree) {
        this.reset();

        for (const node of tree.nodes) {
            for (let copy = 0; copy <= node.stack.length; copy++) {
                this.demo.add(node.key);
            }
        }
    }

    // Each result is the node's id, or null if the operation did not apply.
    add(key) {
        return this.result(this.demo.add(key));
    }

    remove(id) {
        return this.result(this.demo.remove(id));
    }

    find(key) {
        return this.result(this.demo.find(key));
    }

    result(id) {
        return { id: id === NO_NODE ? null : id, frames: this.frames() };
    }

    frames() {
        const { demo } = this;

        return Array.from({ length: demo.frame_count() }, (_, index) => ({
            event: this.eventNames[demo.frame_event(index)],
            node: demo.frame_node(index),
            detail: demo.frame_detail(index),
        }));
    }

    // The tree as plain data. A node's path holds one 0 (left) or 1 (right) per level.
    //
    //   { nodes: [{ id, key, path, stack: [sibling ids] }] }
    snapshot() {
        const nodes = [];
        this.collect(this.demo.root(), '', nodes);
        return { nodes };
    }

    collect(id, path, nodes) {
        if (id === NO_NODE) {
            return;
        }

        const { demo } = this;
        nodes.push({ id, key: demo.key(id), path, stack: this.siblings(id) });
        this.collect(demo.left(id), `${path}0`, nodes);
        this.collect(demo.right(id), `${path}1`, nodes);
    }

    siblings(id) {
        const { demo } = this;
        const stack = [];

        for (let next = demo.next_sibling(id); next !== NO_NODE && next !== id; next = demo.next_sibling(next)) {
            stack.push(next);
        }

        return stack;
    }
}
