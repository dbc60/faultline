// Turns one operation's frames into the steps a player shows. A step is plain data:
//
//   { tree, marks: { role: id }, ghost: { id, key, near } | null, caption }
//
// tree is the snapshot to draw. The tree does not change until the operation's last
// step, because the trace records the search and not each pointer write. A ghost is the
// node being added, drawn beside the node `near` before it joins the tree.

const FIRST_DECISION_BIT = 8;

const sideName = (bit) => (bit === 0 ? 'left' : 'right');

const keyOf = (tree, id) => {
    for (const node of tree.nodes) {
        if (node.id === id || node.stack.includes(id)) {
            return node.key;
        }
    }

    return null;
};

const bitCaption = (key, depth, bit) =>
    `Bit ${FIRST_DECISION_BIT - depth} of ${key} is ${bit}, so go ${sideName(bit)}`;

export function addSteps(before, after, frames, id, key) {
    const place = { tree: after, marks: { result: id }, ghost: null };
    const steps = [];
    let depth = 0;

    for (const frame of frames) {
        switch (frame.event) {
            case 'insert_visit':
                steps.push({
                    tree: before,
                    marks: { visit: frame.node },
                    ghost: { id, key, near: frame.node },
                    caption: `Add ${key}: visit ${keyOf(before, frame.node)}. ${bitCaption(key, depth, frame.detail)}.`,
                });
                depth++;
                break;
            case 'insert_attach':
                steps.push({
                    ...place,
                    caption: `The ${sideName(frame.detail)} link of ${keyOf(before, frame.node)} is free, so ${key} attaches there.`,
                });
                break;
            case 'insert_sibling':
                steps.push({
                    ...place,
                    caption: `${key} is already in the tree, so the new node joins its sibling list and has no tree links.`,
                });
                break;
            case 'insert_root':
                steps.push({ ...place, caption: `The tree is empty, so ${key} becomes the root.` });
                break;
            default:
                throw new Error(`unexpected add frame: ${frame.event}`);
        }
    }

    return steps;
}

export function findSteps(tree, frames, id, key) {
    const steps = [];
    const marks = {};
    let depth = 0;

    const push = (caption) => steps.push({ tree, marks: { ...marks }, ghost: null, caption });

    for (const frame of frames) {
        const nodeKey = keyOf(tree, frame.node);

        switch (frame.event) {
            case 'find_visit':
                marks.visit = frame.node;
                if (nodeKey === key) {
                    push(`Find ${key}: visit ${nodeKey}.`);
                } else {
                    push(`Find ${key}: visit ${nodeKey}. ${bitCaption(key, depth, frame.detail)}.`);
                }
                depth++;
                break;
            case 'find_best':
                marks.best = frame.node;
                push(`${nodeKey} is the best fit so far: ${frame.detail} bytes left over.`);
                break;
            case 'find_save':
                marks.saved = frame.node;
                push(`The search goes left, so save the right subtree at ${nodeKey}. All its keys are larger than ${key}.`);
                break;
            case 'find_dead_end':
                delete marks.visit;
                push(`${nodeKey} has no child on that side, so bit-guided search ends.`);
                break;
            case 'smallest_visit':
                marks.visit = frame.node;
                push(`Walk the saved subtree for its smallest key: visit ${nodeKey}.`);
                break;
            case 'smallest_best':
                marks.smallest = frame.node;
                push(`${nodeKey} is the smallest key in the saved subtree so far.`);
                break;
            default:
                throw new Error(`unexpected find frame: ${frame.event}`);
        }
    }

    const found = id === null ? null : keyOf(tree, id);
    const caption =
        found === null
            ? `No node is large enough for ${key}.`
            : `Best fit for ${key} is ${found}, with ${found - key} bytes left over.`;

    steps.push({ tree, marks: id === null ? {} : { result: id }, ghost: null, caption });

    return steps;
}

export function removeSteps(before, after, frames, id) {
    const key = keyOf(before, id);
    const gone = { tree: after, marks: {}, ghost: null };
    const steps = [{ tree: before, marks: { target: id }, ghost: null, caption: `Remove ${key}.` }];
    let replacement = null;

    for (const frame of frames) {
        switch (frame.event) {
            case 'remove_sibling':
                steps.push({ ...gone, caption: `A sibling has no tree links, so ${key} only leaves the list.` });
                break;
            case 'remove_leaf':
                steps.push({ ...gone, caption: `${key} has no children or siblings, so it leaves the tree.` });
                break;
            case 'replace_from_siblings':
                replacement = frame.node;
                steps.push({
                    tree: before,
                    marks: { target: id, replacement },
                    ghost: null,
                    caption: `${key} has a sibling, and that sibling takes over its place in the tree.`,
                });
                break;
            case 'replace_from_tree':
                replacement = frame.node;
                steps.push({
                    tree: before,
                    marks: { target: id, replacement },
                    ghost: null,
                    caption: `${key} has children, so the leftmost leaf ${keyOf(before, replacement)} replaces it.`,
                });
                break;
            case 'replace_take':
                steps.push({
                    ...gone,
                    marks: { result: replacement },
                    caption:
                        keyOf(before, replacement) === key
                            ? `The sibling now holds the tree position, with the same links.`
                            : `${keyOf(before, replacement)} now holds the position of ${key}, with its links.`,
                });
                break;
            default:
                throw new Error(`unexpected remove frame: ${frame.event}`);
        }
    }

    return steps;
}
