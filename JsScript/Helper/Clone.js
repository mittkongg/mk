export class Clone {
    constructor() {

    };
    cloneObj = function (obj) {
        var newObj = {};
        if (obj instanceof Array) {
            newObj = [];
        }
        for (var key in obj) {
            var val = obj[key];
            newObj[key] = typeof val === 'object' ? this.cloneObj(val) : val;  // 如果是对象，迭代
        }
        return newObj;
    };
};
