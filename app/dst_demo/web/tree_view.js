// Draws steps as an SVG tree. Node elements persist by id and move between steps, and
// edges are redrawn from the nodes' current positions on each animation frame.

import { GHOST_RISE, HEIGHT, NODE_HEIGHT, NODE_WIDTH, WIDTH, layoutTree } from './tree_layout.js';

const SVG_NS = 'http://www.w3.org/2000/svg';
const MARK_ROLES = ['visit', 'best', 'saved', 'smallest', 'target', 'replacement', 'result', 'ghost'];
const STACK_BADGE_MINIMUM = 2;

function svgElement(name, attributes = {}, parent = null) {
    const element = document.createElementNS(SVG_NS, name);

    for (const [attribute, value] of Object.entries(attributes)) {
        element.setAttribute(attribute, value);
    }

    parent?.append(element);
    return element;
}

const easeInOut = (t) => (t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t));
const mix = (from, to, t) => from + (to - from) * t;

export class TreeView {
    // onSelect(id) runs when a reader activates a node.
    constructor(parent, onSelect) {
        this.onSelect = onSelect;
        this.svg = svgElement(
            'svg',
            { viewBox: `0 0 ${WIDTH} ${HEIGHT}`, role: 'group', 'aria-label': 'Digital search tree' },
            parent,
        );
        this.edgeLayer = svgElement('g', {}, this.svg);
        this.siblingLayer = svgElement('g', {}, this.svg);
        this.treeLayer = svgElement('g', {}, this.svg);
        this.nodes = new Map(); // id -> { group, label, badge, x, y, opacity, from, to, gone }
        this.edges = [];
        this.frame = null;
    }

    render(step, ms) {
        const { boxes, edges } = layoutTree(step.tree);
        const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

        if (step.ghost !== null) {
            boxes.set(step.ghost.id, this.ghostBox(step.ghost, boxes));
        }

        this.place(boxes);
        this.mark(step);
        this.drawEdges(edges);
        this.animate(reduced ? 0 : ms);
        this.follow(step, boxes, reduced);
    }

    // On a narrow screen the tree scrolls, so keep the step's active node in view.
    follow(step, boxes, reduced) {
        const scroller = this.svg.parentNode;
        const lastMark = Object.values(step.marks).at(-1);
        const focusId = step.ghost?.near ?? step.marks.visit ?? lastMark ?? step.tree.nodes[0]?.id;

        if (focusId === undefined) {
            return;
        }

        const left = (boxes.get(focusId).x / WIDTH) * scroller.scrollWidth - scroller.clientWidth / 2;
        scroller.scrollTo({ left, behavior: reduced ? 'auto' : 'smooth' });
    }

    ghostBox(ghost, boxes) {
        const near = boxes.get(ghost.near);
        return { x: near.x, y: near.y - GHOST_RISE, key: ghost.key, role: 'tree', count: 1 };
    }

    place(boxes) {
        for (const [id, box] of boxes) {
            const node = this.nodes.get(id) ?? this.createNode(id, box);

            node.gone = false;
            node.from = { x: node.x, y: node.y, opacity: node.opacity };
            node.to = { x: box.x, y: box.y, opacity: 1 };
            node.label.textContent = box.key;
            node.group.setAttribute('aria-label', `Remove chunk of ${box.key} bytes`);
            node.group.classList.toggle('sibling', box.role === 'sibling');
            node.badge.textContent = box.count >= STACK_BADGE_MINIMUM ? `×${box.count}` : '';

            const layer = box.role === 'sibling' ? this.siblingLayer : this.treeLayer;
            if (node.group.parentNode !== layer) {
                layer.append(node.group);
            }
        }

        for (const [id, node] of this.nodes) {
            if (!boxes.has(id)) {
                node.gone = true;
                node.from = { x: node.x, y: node.y, opacity: node.opacity };
                node.to = { ...node.from, opacity: 0 };
            }
        }
    }

    createNode(id, box) {
        const group = svgElement('g', { class: 'node', tabindex: 0, role: 'button' });
        const halfWidth = NODE_WIDTH / 2;
        const halfHeight = NODE_HEIGHT / 2;

        svgElement('rect', { x: -halfWidth, y: -halfHeight, width: NODE_WIDTH, height: NODE_HEIGHT, rx: 4 }, group);
        const label = svgElement('text', { 'text-anchor': 'middle', y: 4 }, group);
        const badge = svgElement('text', { class: 'badge', 'text-anchor': 'middle', y: -halfHeight - 3 }, group);

        group.addEventListener('click', () => this.onSelect(id));
        group.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' || event.key === ' ') {
                event.preventDefault();
                this.onSelect(id);
            }
        });

        const node = { group, label, badge, x: box.x, y: box.y, opacity: 0 };
        this.nodes.set(id, node);
        return node;
    }

    mark(step) {
        const marked = new Map(Object.entries(step.marks).map(([role, id]) => [role, id]));

        if (step.ghost !== null) {
            marked.set('ghost', step.ghost.id);
        }

        for (const [id, node] of this.nodes) {
            for (const role of MARK_ROLES) {
                node.group.classList.toggle(role, marked.get(role) === id);
            }
        }
    }

    drawEdges(edges) {
        this.edgeLayer.replaceChildren();

        this.edges = edges.map((edge) => {
            const line = svgElement('line', {}, this.edgeLayer);
            const bit = svgElement('text', { class: 'bit', 'text-anchor': 'middle' }, this.edgeLayer);

            bit.textContent = edge.bit;
            return { ...edge, line, bit };
        });
    }

    animate(ms) {
        cancelAnimationFrame(this.frame);
        const start = performance.now();

        const tick = (now) => {
            // A frame's timestamp can precede the performance.now() taken above.
            const t = ms === 0 ? 1 : Math.min(Math.max(now - start, 0) / ms, 1);

            this.applyProgress(easeInOut(t));

            if (t < 1) {
                this.frame = requestAnimationFrame(tick);
            } else {
                this.dropGone();
            }
        };

        // The first frame runs now, so edges never appear at stale positions.
        tick(start + (ms === 0 ? 0 : 1));
    }

    applyProgress(t) {
        for (const node of this.nodes.values()) {
            node.x = mix(node.from.x, node.to.x, t);
            node.y = mix(node.from.y, node.to.y, t);
            node.opacity = mix(node.from.opacity, node.to.opacity, t);
            node.group.setAttribute('transform', `translate(${node.x} ${node.y})`);
            node.group.setAttribute('opacity', node.opacity);
        }

        for (const edge of this.edges) {
            const from = this.nodes.get(edge.from);
            const to = this.nodes.get(edge.to);

            edge.line.setAttribute('x1', from.x);
            edge.line.setAttribute('y1', from.y);
            edge.line.setAttribute('x2', to.x);
            edge.line.setAttribute('y2', to.y);
            edge.bit.setAttribute('x', (from.x + to.x) / 2);
            edge.bit.setAttribute('y', (from.y + to.y) / 2 - 3);
        }
    }

    dropGone() {
        for (const [id, node] of this.nodes) {
            if (node.gone) {
                node.group.remove();
                this.nodes.delete(id);
            }
        }
    }
}
