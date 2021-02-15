var BattleData = {
    bw: 60,
    bh: 60
};
var Player = require('Player');

import { Queue } from './Helper/Queue';
const BeHaveTree = require('./Helper/BehaveTree')

var BattleFiled = cc.Class({
    extends: cc.Component,
    properties: {
        batterFiledPrefab: cc.Prefab,
        rangeFiledPrefab: cc.Prefab,
        field: new Array(),
        msgQue: {
            default: null,
            type: Queue,
        },
        objs: new Array(),
    },
    ctor: function () {
        this.msgQue = new Queue();
        this.ready = false;
        this.loadRes();

        this.behaveTreeRoot = new BeHaveTree.TreeNode();
        this.SelectNode_1 = new BeHaveTree.SequnceNode(this.behaveTreeRoot);
        this.SelectSeqNode_2 = new BeHaveTree.SelectSeq(this.SelectNode_1);
        this.SequnceSelNode_3 = new BeHaveTree.SequnceSel(this.SelectSeqNode_2);
        this.SelectSelNode_4_1 = new BeHaveTree.SelectSel(this.SequnceSelNode_3);
        this.SelectActNode_4_2 = new BeHaveTree.SelectAct(this.SequnceSelNode_3);
        this.SelectActNode_5_1 = new BeHaveTree.SelectAct(this.SelectSelNode_4_1);
        //this.WalkLeafNode_1_1 = new BeHaveTree.SequnceWalkLeafNode(this.SequnceNode_1);
        //this.AttackLeafNode_1_2 = new BeHaveTree.SequnceAttackLeafNode(this.SequnceNode_1);

        this.ebehaveTreeRoot = new BeHaveTree.TreeNode();
        this.eSelectNode_1 = new BeHaveTree.SequnceNode(this.ebehaveTreeRoot);
        this.eSelectSeqNode_2 = new BeHaveTree.SelectSeq(this.eSelectNode_1);
        this.eSequnceSelNode_3 = new BeHaveTree.SequnceSel(this.eSelectSeqNode_2);
        this.eSelectSelNode_4_1 = new BeHaveTree.SelectSel(this.eSequnceSelNode_3);
        this.eSelectActNode_4_2 = new BeHaveTree.SelectAct(this.eSequnceSelNode_3);
        this.eSelectActNode_5_1 = new BeHaveTree.SelectAct(this.eSelectSelNode_4_1);



        
        
    },
    mainLoop: function(msgQue, objs) {
        if (!msgQue.isEmpty()) {
            let msg = que.front();
            for (obj in objs) {

            }
        }
        if (this.ready) {
            if (this.behaveTreeRoot.travelOver == false) {
                this.behaveTreeRoot.travelOver = 2; // means the middle status
                cc.log("travel 1");
                this.behaveTreeRoot.travel(this.behaveTreeRoot, this.behaveTreeRoot);
            }

            if (this.behaveTreeRoot.travelOver == true) {
                this.behaveTreeRoot.reset(this.behaveTreeRoot);
                this.behaveTreeRoot.travelOver = false;
            }
            ////////////////////////////////////////////////////////////////////////
            
            if (this.ebehaveTreeRoot.travelOver == false) {
                this.ebehaveTreeRoot.travelOver = 2; // means the middle status
                this.ebehaveTreeRoot.travel(this.ebehaveTreeRoot, this.ebehaveTreeRoot);
            }

            if (this.ebehaveTreeRoot.travelOver == true) {
                this.ebehaveTreeRoot.reset(this.ebehaveTreeRoot);
                this.ebehaveTreeRoot.travelOver = false;
            }

        }
    },
    // use this for initialization
    loadRes: function () {
        var node = cc.find("Canvas/Root");
        var self = this;
        cc.resources.load("floor", cc.Prefab, (error, prefab) => {
            if (!error) {
                self.batterFiledPrefab = cc.instantiate(prefab);
                cc.resources.load("attack_range", cc.Prefab, (error, prefab) => {
                    if (!error) {
                        self.rangeFiledPrefab = cc.instantiate(prefab);
                        cc.resources.load("block", cc.Prefab, (error, prefab) => {
                            if (!error) {
                                self.blockPrefab = cc.instantiate(prefab);
                                self.draw(node);
                                // field_bound['max_x'] = self.field['row_n'];
                                // field_bound['max_y'] = self.field['col_n'];
                                // field_bound['bd_w'] = self.field['bd_w'];
                                // field_bound['bd_h'] = self.field['bd_h'];

                                let testA = new Player();
                                self.objs.push(testA);
                                testA.registBattleField(self);
                                testA.physical_damage = 15;
                                testA.loadRes(0, 0, 6, 3, (self.field));
                                testA.setAttri([1, 0]);
                                // let testB = new Player();
                                // self.objs.push(testB);
                                // testB.registBattleField(self);
                                // testB.setAttri([1, 0]);
                                // testB.loadRes(9, 9, 0, 0, (self.field));
                                let testC = new Player();
                                self.objs.push(testC);
                                testC.registBattleField(self);
                                testC.setAttri([1, 1]);
                                testC.loadRes(6, 3, 0, 0, (self.field));
                                
                                


                                self.SelectSeqNode_2.registExcute(
                                    function log() {
                                        return self.objs[0].haveEnemy();
                                    }
                                )
                                self.SequnceSelNode_3.registExcute(
                                    function log() {
                                        return self.objs[0].haveAttactPath();
                                    }
                                )
                                self.SelectSelNode_4_1.registExcute(
                                    function log() {
                                        return self.objs[0].haveAttactRange();
                                    }
                                )
                        
                                self.SelectActNode_4_2.registExcute(
                                    function log() {
                                        return self.objs[0].actWalk(self.SelectActNode_4_2.actioning);
                                    }
                                )
                        
                                self.SelectActNode_5_1.registExcute(
                                    function log() {
                                        return self.objs[0].actAttack(self.SelectActNode_5_1.actioning);
                                    }
                                )

                                //////////////////////////////////////////////////
                                self.eSelectSeqNode_2.registExcute(
                                    function log() {
                                        return self.objs[1].haveEnemy();
                                    }
                                )
                                self.eSequnceSelNode_3.registExcute(
                                    function log() {
                                        return self.objs[1].haveAttactPath();
                                    }
                                )
                                self.eSelectSelNode_4_1.registExcute(
                                    function log() {
                                        return self.objs[1].haveAttactRange();
                                    }
                                )
                        
                                self.eSelectActNode_4_2.registExcute(
                                    function log() {
                                        return self.objs[1].actWalk(self.SelectActNode_4_2.actioning);
                                    }
                                )
                        
                                self.eSelectActNode_5_1.registExcute(
                                    function log() {
                                        return self.objs[1].actAttack(self.SelectActNode_5_1.actioning);
                                    }
                                )

                                
                            }
                        });
                    }
                });
                
                
                

            }
        });




    },
    showAttackRange: function(player) {
        for (let i = 0; i < player.attack_range.length; i++) {
            let a_r_x = player.x_idx + player.attack_range[i][0];
            let a_r_y = player.y_idx + player.attack_range[i][1];
            if (a_r_x < 0 || a_r_y < 0 || a_r_x >= this.field['row_n'] || a_r_y >= this.field['col_n']) {
                return;
            }
            this.field[a_r_x][a_r_y].blockFiled.zIndex = 0;
            this.field[a_r_x][a_r_y].rangeFiled.zIndex = 2;
            this.field[a_r_x][a_r_y].zIndex = 1;
            setTimeout(()=>{
                this.field[a_r_x][a_r_y].rangeFiled.zIndex = 1;
                this.field[a_r_x][a_r_y].zIndex = 2;
                this.field[a_r_x][a_r_y].blockFiled.zIndex = 0;
            }, 500);
        }
    },

    showBlockRange: function(sw, x_idx, y_idx) {
        if (sw) {
            this.field[x_idx][y_idx].blockFiled.zIndex = 2;
            this.field[x_idx][y_idx].rangeFiled.zIndex = 0;
            this.field[x_idx][y_idx].zIndex = 1;
        } else {
            this.field[x_idx][y_idx].blockFiled.zIndex = 0;
            this.field[x_idx][y_idx].rangeFiled.zIndex = 1;
            this.field[x_idx][y_idx].zIndex = 2;
        }
    },

    calcGap: function () {
        let size = cc.view.getFrameSize();
        let numw = Math.floor(size.width / BattleData.bw);
        let exceed_w = size.width - numw * BattleData.bw;
        let numh = Math.floor(size.height / BattleData.bh);
        let exceed_h = size.height - numh * BattleData.bh;
        let gap = [];
        gap['w'] = Math.floor(exceed_w / (numw - 1));
        gap['h'] = Math.floor(exceed_h / (numh - 1));
        gap['row_n'] = numw;
        gap['col_n'] = numh;
        return gap;
    },
    updateBf: function() {
        for (let idx_x = 0; idx_x < this.field['row_n']; idx_x++) {
            for (let idx_y = 0; idx_y < this.field['col_n']; idx_y++) {
                if (this.field[idx_x][idx_y].visit == 1) {
                    this.showBlockRange(1, idx_x, idx_y);
                } else {
                    this.showBlockRange(0, idx_x, idx_y);
                }
            }
        }
    },

    draw: function (node) {
        let size = cc.view.getFrameSize();
        let gap = this.calcGap();
        let x_move = BattleData.bw + gap['w'];
        let y_move = BattleData.bh + gap['h'];
        for (let x = 0, idx_x = 0; x < size.width; x += x_move, idx_x++) {
            let center_x = x + BattleData.bw / 2;
            this.field[idx_x] = new Array();
            for (let y = 0, idx_y = 0; y < size.height; y += y_move, idx_y++) {
                let center_y = y + BattleData.bh / 2;
                var batterFiled = cc.instantiate(this.batterFiledPrefab);
                var rangeFiled = cc.instantiate(this.rangeFiledPrefab);
                var blockFiled = cc.instantiate(this.blockPrefab);
                node.addChild(batterFiled);
                node.addChild(rangeFiled);
                node.addChild(blockFiled);
                blockFiled.zIndex = 0;
                rangeFiled.zIndex = 1;
                batterFiled.zIndex = 2;
                blockFiled.setPosition(center_x, center_y);
                batterFiled.setPosition(center_x, center_y);
                rangeFiled.setPosition(center_x, center_y);
                this.field[idx_x][idx_y] = batterFiled;
                this.field[idx_x][idx_y].blockFiled = blockFiled;
                this.field[idx_x][idx_y].rangeFiled = rangeFiled;
                this.field[idx_x][idx_y].visit = 0;
                this.field[idx_x][idx_y].pos = [center_x, center_y];
                // cc.log(idx_x + ',' + idx_y);
            }
            //break;
        }
        this.field['row_n'] = gap['row_n'];
        this.field['col_n'] = gap['col_n'];
        this.field['bd_w'] = BattleData.bw;
        this.field['bd_h'] = BattleData.bh;
    },

    // called every frame
    update: function (dt) {
        // cc.log(dt);
    },
});


export default BattleFiled;