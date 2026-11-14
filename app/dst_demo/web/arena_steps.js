// Turns a trace from arena_trace.exe into the steps of steps.js, for one large bin.
//
// Tree frames carry no snapshot, so they come in runs that end at an arena frame. The
// run goes to addSteps, findSteps or removeSteps with the trees on either side of it:
//
//   insert_visit, insert_visit, insert_attach, large_insert   -> addSteps
//   find_visit, find_best, ... , find_dead_end                -> findSteps
//   replace_from_tree, replace_take, large_remove             -> removeSteps
//
// A trace for this view stays in one bin: see large_bins.txt.

import { addSteps, findSteps, removeSteps } from './steps.js';

// A note holds longer than a step, because a reader has more to read.
const NOTE_HOLD = 4;

const INSERT_EVENTS = ['insert_visit', 'insert_attach', 'insert_sibling', 'insert_root'];
const FIND_EVENTS = ['find_visit', 'find_best', 'find_save', 'find_dead_end', 'smallest_visit', 'smallest_best'];
const REMOVE_EVENTS = ['replace_from_tree', 'replace_from_siblings', 'replace_take', 'remove_leaf', 'remove_sibling'];

const ARENA_CAPTIONS = {
    path_exact_fit: () => 'A small bin holds a chunk that fits with nothing to split.',
    path_fast: () => 'The fast node is large enough, so the request comes from it.',
    path_next_small_bin: () => 'The next non-empty small bin supplies the chunk.',
    path_large_for_small: () => 'No small chunk fits, so a large bin supplies one.',
    path_large_bin: () => 'No fast node fits, so search the large bin for this size.',
    path_top: () => 'No free chunk fits, so the request comes from top, the unused end of the region.',
    path_extend: () => 'Top is too small, so the region grows.',
    small_insert: (frame) => `The ${frame.size}-byte chunk goes in a small bin, not this tree.`,
    split: (frame) => `The chunk is larger than the request, so ${frame.size} bytes split off.`,
    merge_back: (frame) => `The chunk merges with its free neighbor below, making ${frame.size} bytes.`,
    merge_forward: (frame) => `The chunk merges with its free neighbor above, making ${frame.size} bytes.`,
    merge_top: () => 'The chunk borders top, so top absorbs it and no bin changes.',
};

export function arenaSteps(trace, binIndex = 0) {
    const steps = [];
    const { frames } = trace;
    let tree = { nodes: [] };
    let name = '';
    let request = 0;
    let index = 0;

    const treeOf = (state) => ({
        nodes: state.large.find((bin) => bin.index === binIndex)?.nodes ?? [],
    });

    // A note step has note: true, so a view can set the narration apart.
    const push = (caption, extra = {}) => steps.push({ tree, marks: {}, ghost: null, caption, ...extra });

    // Collects the run of events that starts at the current frame.
    const takeRun = (events) => {
        const run = [];

        while (events.includes(frames[index].event)) {
            run.push(frames[index++]);
        }

        return run;
    };

    while (index < frames.length) {
        const frame = frames[index];

        if (frame.state !== undefined) {
            tree = treeOf(frame.state);
        }

        if (INSERT_EVENTS.includes(frame.event)) {
            const run = takeRun(INSERT_EVENTS);
            const end = frames[index];
            const after = treeOf(end.state);

            steps.push(...addSteps(tree, after, run, end.chunk, end.size));
            tree = after;
            index++;
        } else if (FIND_EVENTS.includes(frame.event)) {
            const run = takeRun(FIND_EVENTS);
            const removal = frames.slice(index).find((later) => later.event === 'large_remove');

            steps.push(...findSteps(tree, run, removal.chunk, request));
        } else if (REMOVE_EVENTS.includes(frame.event)) {
            const run = takeRun(REMOVE_EVENTS);
            const end = frames[index];
            const after = treeOf(end.state);

            steps.push(...removeSteps(tree, after, run, end.chunk));
            tree = after;
            index++;
        } else {
            switch (frame.event) {
                case 'title':
                    break;
                case 'note':
                    push(frame.text, { hold: NOTE_HOLD, note: true });
                    break;
                case 'op_malloc':
                    ({ name } = frame);
                    request = frame.size;
                    push(`${name} = malloc(${frame.request}) needs a chunk of ${frame.size} bytes.`);
                    break;
                case 'op_free':
                    push(`free(${frame.name}) returns a chunk of ${frame.size} bytes.`);
                    break;
                case 'allocated':
                    push(`${name} now holds a chunk of ${frame.size} bytes.`);
                    break;
                default: {
                    const caption = ARENA_CAPTIONS[frame.event];

                    if (caption === undefined) {
                        throw new Error(`unexpected trace frame: ${frame.event}`);
                    }

                    push(caption(frame));
                    break;
                }
            }

            index++;
        }
    }

    return steps;
}

export const traceTitle = (trace) => trace.frames.find((frame) => frame.event === 'title')?.text ?? '';
