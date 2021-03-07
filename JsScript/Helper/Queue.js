export class Queue {
    constructor() {
        this.items = [];
    }
    enqueue = function (elememnt) {
        this.items.push(elememnt);
    };
    dequeue = function () {
        return this.items.shift();
    };
    front = function () {
        return this.items[0];
    };
    isEmpty = function () {
        return this.items.length === 0;
    };
    size = function () {
        return this.items.length;
    };
    print = function () {
        console.log(this.items.toString());
    };
};
