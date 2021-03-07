export default cc.Class({
    extends: cc.Component,

    properties: {
        info: cc.Label,
        fileProgress: cc.ProgressBar,
        fileLabel: cc.Label,
        byteProgress: cc.ProgressBar,
        byteLabel: cc.Label,
        debugText: cc.Label,
        close: cc.Node,
        // checkBtn: cc.Node,
        // retryBtn: cc.Node,
        // updateBtn: cc.Node
    },
    
    onLoad () {
        cc.director.preloadScene("BattleField");
        this.close.on(cc.Node.EventType.TOUCH_END, function () {
            cc.director.loadScene("BattleField");
        }, this);
    }
});