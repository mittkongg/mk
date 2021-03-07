var GlobalJumpWord = cc.Class({
    extends: cc.Component,
    properties: {

    },
    // use this for initialization
    onLoad: function () {


    },
    // called every frame
    update: function (dt) {

    },

    hurt: function (parent, str, posx, posy) {
        var node = new cc.Node();
        var label = node.addComponent(cc.Label);
        label.string = "-" + str;
        label.fontSize = 50;
        node.color = new cc.color(255, 0, 0);
        node.parent = parent;
        node.x = 0;
        node.y = 50;

        setTimeout(() => {
            cc.tween(node)
                .by(0.5, { scale: 0.8, position: cc.v2(0, 100) })
                .to(0.5, { opacity: 0 })
                .start();
            
            cc.tween(parent)
                .by(0.01, { position: cc.v2(-8, 0) })
                .by(0.01, { position: cc.v2(0, 8) })
                .by(0.01, { position: cc.v2(8, 0) })
                .by(0.01, { position: cc.v2(0, -8) })
                .start();
        }, 100);
    },

    statics: {
        _instance: null,
    },
});
GlobalJumpWord._instance = new GlobalJumpWord();
export default GlobalJumpWord;
