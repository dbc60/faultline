// Lists the captions of the steps a player has shown, from the first step to the current
// one, so earlier text stays readable. Stepping back removes the later entries.

export class StepLog {
    constructor(list) {
        this.list = list;
        this.steps = null;
    }

    // Called after the player moves, with its steps and the index it now shows.
    render(steps, index) {
        if (steps !== this.steps) {
            this.steps = steps;
            this.list.replaceChildren();
        }

        // Remove entries past the current step, then add any missing up to it. Only
        // additions reach a screen reader, because the list has role="log".
        while (this.list.children.length > index + 1) {
            this.list.lastElementChild.remove();
        }

        while (this.list.children.length <= index) {
            this.list.append(this.entry(steps[this.list.children.length]));
        }

        this.list.querySelector('.current')?.classList.remove('current');
        this.list.lastElementChild?.classList.add('current');
        this.list.scrollTop = this.list.scrollHeight;
    }

    entry(step) {
        const item = document.createElement('li');

        item.textContent = step.caption;

        if (step.note) {
            item.classList.add('note');
        }

        return item;
    }
}
