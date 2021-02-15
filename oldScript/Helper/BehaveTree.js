
class TreeNode {
    constructor() {
        this.father = null;
        this.children = [];
        this.goon = true;
        this.actioning = false;
        this.travelOver = false;
    };


    reset = function (node) {
        for (let childIdx = 0; childIdx < node.children.length; childIdx++) {
            let child = node.children[childIdx];
            child.father.goon = true;
            if (child.children.length != 0) {
                this.reset(child);
            }
        }
    };


    travel = function (node, root) {

        for (let childIdx = 0; childIdx < node.children.length; childIdx++) {
            let child = node.children[childIdx];
            if (child.father.goon && !child.actioning) {
                let childGoon = child.excute(child.actioning);
                if (childGoon && child.children.length != 0) {
                    this.travel(child, root);
                }
            }
        }
        if (node == root) {
            this.travelOver = true;
        }
    };

};

class SequnceNode extends TreeNode {
    // father
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);

    };

    excute = function () {
        return true;
    };
};

class SequnceSeq extends TreeNode {
    // father
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);

    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc();
            this.father.goon = tf;
        }
        return tf;
    };
};

class SequnceSel extends TreeNode {
    // father
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);

    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc(this.actioning);
            this.father.goon = tf;
        }
        return tf;
    };
};

class SequnceAct extends TreeNode {
    // father
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);

    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc();
            this.father.goon = tf;
        }
        return tf;
    };
};

class SelectNode extends TreeNode {
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);
    };
};

class SelectSeq extends TreeNode {
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);
    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc();
            this.father.goon = !tf;
        }
        return tf;
    };
};

class SelectSel extends TreeNode {
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);
    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc();
            this.father.goon = !tf;
        }
        return tf;
    };
};

class SelectAct extends TreeNode {
    constructor(node) {
        super();
        this.father = node;
        this.father.children.push(this);
    };

    registExcute = function (f) {
        this.excuteFunc = f;
    };

    excute = function () {
        let tf = false;
        if (this.father.goon) {
            tf = this.excuteFunc();
            this.father.goon = !tf;
        }
        return tf;
    };
};

exports.TreeNode = TreeNode;
exports.SequnceNode = SequnceNode;
exports.SequnceAct = SequnceAct;
exports.SequnceSeq = SequnceSeq;
exports.SequnceSel = SequnceSel;
exports.SelectNode = SelectNode;
exports.SelectSeq = SelectSeq;
exports.SelectSel = SelectSel;
exports.SelectAct = SelectAct;
