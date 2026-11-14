// Plays a list of steps through any number of views. It knows nothing about what a step
// holds, so scripted traces and free play share it. A view is { render(step, ms) }.
//
// Pausing lasts across load(), so a reader can step through each new operation by hand.
// A step's optional hold multiplies the delay before the next one.

export class Player {
    constructor(views, onChange) {
        this.views = views;
        this.onChange = onChange;
        this.steps = [];
        this.index = 0;
        this.delay = 900;
        this.paused = false;
        this.timer = null;
    }

    get atEnd() {
        return this.index >= this.steps.length - 1;
    }

    get running() {
        return this.timer !== null;
    }

    load(steps) {
        this.stop();
        this.steps = steps;
        this.show(0);
        this.schedule();
        this.onChange();
    }

    play() {
        this.paused = false;

        if (this.atEnd) {
            this.show(0);
        }

        this.schedule();
        this.onChange();
    }

    pause() {
        this.paused = true;
        this.stop();
        this.onChange();
    }

    next() {
        this.pause();
        this.show(Math.min(this.index + 1, this.steps.length - 1));
    }

    previous() {
        this.pause();
        this.show(Math.max(this.index - 1, 0));
    }

    show(index) {
        this.index = index;
        const ms = Math.min(this.delay * 0.6, 450);

        for (const view of this.views) {
            view.render(this.steps[index], ms);
        }

        this.onChange();
    }

    schedule() {
        if (this.paused || this.atEnd) {
            return;
        }

        const hold = this.steps[this.index].hold ?? 1;

        this.timer = setTimeout(() => {
            this.timer = null;
            this.show(this.index + 1);
            this.schedule();
            this.onChange();
        }, this.delay * hold);
    }

    stop() {
        clearTimeout(this.timer);
        this.timer = null;
    }
}
