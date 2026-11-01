// Places a snapshot's nodes. A node's x comes from its left/right path, so it stays put
// as nodes come and go around it, and only a node that changes place moves:
//
//   level 0            root                   x = 1/2 of the width
//   level 1      0            1               x = 1/4, 3/4
//   level 2   00   01      10   11            x = 1/8, 3/8, 5/8, 7/8

export const WIDTH = 1280;
export const NODE_WIDTH = 34;
export const NODE_HEIGHT = 24;

const TOP = 58;
const LEVEL_HEIGHT = 84;
const LEVELS = 6; // the 32 keys of a bin need the root and five more levels
const BOTTOM_MARGIN = 40;

export const HEIGHT = TOP + (LEVELS - 1) * LEVEL_HEIGHT + BOTTOM_MARGIN;

// Siblings show as offset cards behind their tree node. The rest share the last card.
const STACK_OFFSET = 5;
const VISIBLE_STACK = 2;

export const GHOST_RISE = NODE_HEIGHT + 6;

function pathPosition(path) {
    let fraction = 0.5;
    let width = 0.25;

    for (const bit of path) {
        fraction += bit === '1' ? width : -width;
        width /= 2;
    }

    return { x: fraction * WIDTH, y: TOP + path.length * LEVEL_HEIGHT };
}

const parentPath = (path) => path.slice(0, -1);

// Returns boxes by id, plus the edges between tree nodes. A box's role says whether it
// is a tree node or a sibling behind one.
export function layoutTree(tree) {
    const boxes = new Map();
    const idByPath = new Map();
    const edges = [];

    for (const node of tree.nodes) {
        const position = pathPosition(node.path);

        boxes.set(node.id, { ...position, key: node.key, role: 'tree', count: node.stack.length + 1 });
        idByPath.set(node.path, node.id);

        node.stack.forEach((id, index) => {
            const offset = STACK_OFFSET * Math.min(index + 1, VISIBLE_STACK);
            boxes.set(id, {
                x: position.x + offset,
                y: position.y + offset,
                key: node.key,
                role: 'sibling',
                count: 1,
            });
        });
    }

    for (const node of tree.nodes) {
        if (node.path !== '') {
            edges.push({ from: idByPath.get(parentPath(node.path)), to: node.id, bit: node.path.at(-1) });
        }
    }

    return { boxes, edges };
}
