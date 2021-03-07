
import BattleField from "BattleField";

cc.Class({
    extends: cc.Component,
    properties: {
        
    },
    // use this for initialization
    onLoad: function () {

        let bf = new BattleField();
        this.bf = bf;
        this.dt = 0;
        
    },
    // called every frame
    update: function (dt) {
        this.dt += dt;
        if (this.dt > 1.1) {
            this.dt = 0;
            this.bf.mainLoop(this.bf.msgQue, this.bf.objs);
        }
    },
});

