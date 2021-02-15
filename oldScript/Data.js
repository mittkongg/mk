// Learn cc.Class:
//  - https://docs.cocos.com/creator/manual/en/scripting/class.html
// Learn Attribute:
//  - https://docs.cocos.com/creator/manual/en/scripting/reference/attributes.html
// Learn life-cycle callbacks:
//  - https://docs.cocos.com/creator/manual/en/scripting/life-cycle-callbacks.html
var PlayerAttackType = cc.Enum({
    Normal: 0,
});
var PlayerAttri = cc.Enum({
    PhysicalDamage: 0,
});
var PlayerRelMsg = cc.Enum({
    POS: 0,
    ATTACK: 1,
});
var GlobalData = cc.Class({
    extends: cc.Component,

    properties: {
        // foo: {
        //     // ATTRIBUTES:
        //     default: null,        // The default value will be used only when the component attaching
        //                           // to a node for the first time
        //     type: cc.SpriteFrame, // optional, default is typeof default
        //     serializable: true,   // optional, default is true
        // },
        // size: {
        //     get () {
        //         return this.size;
        //     },
        //     set (value) {
        //         this.size = value;
        //     }
        // },
        player_attack_type: {
            default: PlayerAttackType.Normal,
            type: PlayerAttackType,
        },
        player_attri_type: {
            default: PlayerAttri.PhysicalDamage,
            type: PlayerAttri,
        },
        player_relmsg_type: {
            default: PlayerRelMsg.POS,
            type: PlayerRelMsg,
        },
    },

    // LIFE-CYCLE CALLBACKS:

    // onLoad () {},
    relmsgPos() {
        return PlayerRelMsg[0];
    },
    relmsgAttack() {
        return PlayerRelMsg[1];
    },
    start () {

    },

    statics: {
        _instance: null,
    },
    // update (dt) {},
});
GlobalData._instance = new GlobalData();
export default GlobalData;
