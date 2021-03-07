import { SearchAlgo } from './Helper/SearchAlgo';
var GlobalData = require("Data");
var GlobalJumpWord = require("./Helper/JumpWord");
var Player = cc.Class({
    extends: cc.Component,
    properties: {
        // 声明 player 属性
        m_id: 0,
        group_id: 0, //不同group才会互相攻击
        target_id: -1, // 未初始化敌对目标
        x_idx: 0,
        y_idx: 0,
        move_action_status: 0,
        attack_range: new Array(),

        move_speed: 5,
        physical_damage: 10,
        health_value: 100,
        attack_interval: 1000,
    },
    statics: {
        id: 0,
        resCount: 0,
        boardcastMsg: function (self, msgType) {
            if (self.root != undefined) {
                self.root.emit(msgType, {
                    obj: self,
                });
            }
        },
        receiveMsg: function (self) {
            if (self.root != undefined) {
                self.root.on(GlobalData._instance.relmsgPos(), function (event) {
                    let msgObj = event.obj;
                    // 广播消息，所以可能收到自己的obj，也有可能是对方的
                    if (msgObj.m_id != self.m_id) {
                        // self.field[msgObj.x_idx][msgObj.y_idx].visit = 1;
                        self.bf.showBlockRange(1, msgObj.x_idx, msgObj.y_idx);
                        if (self.move_action_status != 0) {
                            console.log("id :" + msgObj.m_id + " received but i'm actioning");
                            return;
                        }
                        // 未锚定目标
                        if (self.target_id == -1) {
                            if (self.group_id != msgObj.group_id) {
                                self.target_id = msgObj.m_id;
                            }
                        } else {
                            if (msgObj.m_id != self.target_id) {
                                return;
                            }
                        }

                        if ((msgObj.group_id != self.group_id) &&
                            self.isInAttackRange(msgObj.x_idx, msgObj.y_idx) &&
                            (msgObj.health_value > 0)) {
                            console.log(msgObj.m_id + " in " + self.m_id + " attack Ragne ");
                            //setTimeout(() => {
                            //    if (msgObj.health_value > 0) {
                            Player.boardcastMsg(msgObj, GlobalData._instance.relmsgAttack());
                            Player.boardcastMsg(self, GlobalData._instance.relmsgPos());
                            //    }
                            //}, self.attack_interval);


                            return;
                        }
                        let algo = new SearchAlgo();
                        let tar_idx = self.getTargetFieldIdx(self, msgObj.x_idx, msgObj.y_idx);
                        let find_a_way = algo.search(self.x_idx, self.y_idx, tar_idx[0], tar_idx[1], self.field);
                        if (find_a_way) {
                            if (self.move_action != undefined) {
                                // action done
                                if (self.move_action_status == 0) {
                                    self.move_action.stop();
                                    self.recursionMoveAction(self, algo.walk_steps, algo.walk_steps.length - 2);
                                    console.log("id :" + self.m_id + " . move to :" + msgObj.x_idx + " " + msgObj.y_idx);
                                } else {
                                    console.log("id :" + self.m_id + " actioning");
                                    // node.move_action.stop();
                                }
                            } else {
                                self.recursionMoveAction(self, algo.walk_steps, algo.walk_steps.length - 2);
                                console.log("id :" + self.m_id + "  action not defined");
                            }

                        } else {
                            console.log("id :" + self.m_id + " cant find a way " + msgObj.m_id);
                        }
                    }
                });
                //ATTACK
                self.root.on(GlobalData._instance.relmsgAttack(), function (event) {
                    let msgObj = event.obj;
                    // 广播消息，所以可能收到自己的obj，也有可能是对方的
                    if ((msgObj.m_id != self.m_id) && (msgObj.group_id != self.group_id)) {
                        Player.boardcastMsg(self, GlobalData._instance.relmsgPos());
                        msgObj.health_value -= self.physical_damage;
                        GlobalJumpWord._instance.hurt(msgObj.player_node, self.physical_damage, msgObj.player_node.x, msgObj.player_node.y);
                        // 目标死亡
                        if (msgObj.health_value <= 0) {
                            console.log(msgObj.m_id + " died");
                            cc.tween(msgObj.player_node)
                                .to(1, { opacity: 0 })
                                .start();
                            self.target_id = -1;
                        } else {
                            let progress = msgObj.health_value / 100;
                            msgObj.progressBar.progress = progress;
                            console.log(msgObj.m_id + " get damage " + msgObj.physical_damage);
                        }
                    }

                });
            }
        },
    },
    ctor() {
        this.m_id = Player.id++;
        this.initAttackRange();
    },
    registBattleField(bf) {
        this.bf = bf;
    },
    initAttackRange() {
        this.attack_range[0] = [1, 0];
        this.attack_range[1] = [-1, 0];
        this.attack_range[2] = [0, 1];
        this.attack_range[3] = [0, -1];
    },

    setAttri: function (d) {
        this.move_speed = d[0];
        this.group_id = d[1];
    },
    // 行为树 相关判断函数
    haveEnemy: function() {
        var self = this;
        self.enemy = [];
        for (let objIdx = 0; objIdx < self.bf.objs.length; objIdx++) {
            let obj = self.bf.objs[objIdx];
            if ((this.health_value > 0) &&
                (obj.m_id != this.m_id) &&
                (obj.group_id != this.group_id) &&
                (obj.health_value > 0)) {
                self.enemy.push(obj);
            }
        }
        if (self.enemy.length == 0) {
            return false;
        }
        return true;
    },

    haveAttactPath: function() {
        var self = this;
        if (self.enemy.length != 0) {
            var mainEnemy = self.enemy[0];
            let algo = new SearchAlgo();
            self.algo = algo;
            let tar_idx = self.getTargetFieldIdx(self, mainEnemy.x_idx, mainEnemy.y_idx);
            let hasPath = algo.search(self.x_idx, self.y_idx, tar_idx[0], tar_idx[1], self.field);
            self.pathQue = hasPath[1];
            self.mainEnemy = mainEnemy;
            return hasPath[0];
        }
        self.pathQue = [];
        self.algo = null;
        self.mainEnemy = null;
        return false;
    },

    haveAttactRange: function () {
        var self = this;
        if (self.mainEnemy != null) {
            return self.isInAttackRange(self.mainEnemy.x_idx, self.mainEnemy.y_idx);
        }
        return false;
    },

    actAttack: function (actioning) {
        var self = this;
        if (self.mainEnemy != null) {
            actioning = true;
            self.mainEnemy.health_value -= self.physical_damage;
            GlobalJumpWord._instance.hurt(self.mainEnemy.player_node, self.physical_damage,
                self.mainEnemy.player_node.x, self.mainEnemy.player_node.y);
            
            if (self.mainEnemy.health_value <= 0) {
                console.log(self.mainEnemy.m_id + " died");
                cc.tween(self.mainEnemy.player_node)
                    .to(1, { opacity: 0 })
                    .call(() => {
                        actioning = false;
                    })
                    .start();
                // self.target_id = -1;
            } else {
                let progress = self.mainEnemy.health_value / 100;
                self.mainEnemy.progressBar.progress = progress;
                console.log(self.mainEnemy.m_id + " get damage " + self.mainEnemy.physical_damage);
            }
        }

    },

    actWalk: function (actioning) {
        if (actioning) {
            return;
        }
        var self = this;
        if (self.algo != null) {
            let idx = self.algo.walk_steps.length - 2;
            if (idx < 0) {
                console.log(self.m_id + " error idx " + idx);
                return;
            }
            self.move_action_status = 1;
            self.field[self.x_idx][self.y_idx].visit = 0;
            self.bf.showBlockRange(0, self.x_idx, self.y_idx);
            self.bf.updateBf();
            actioning = true;
            cc.log(self.m_id + ' walk ');
            self.move_action = cc.tween(this.player_node)
                .to(1 / self.move_speed, { position: cc.v2(self.algo.walk_steps[idx][0], self.algo.walk_steps[idx][1]), angle: 0 })
                .call(() => {
                    self.x_idx = self.algo.walk_steps[idx].x;
                    self.y_idx = self.algo.walk_steps[idx].y;
                    self.bf.showBlockRange(1, self.x_idx, self.y_idx);
                    self.move_action_status = 0;
                    self.bf.showAttackRange(self);
                    actioning = false;

                })
                .start()
        }
    },
    // end 行为树 相关判断函数
    isInAttackRange: function (tar_x, tar_y) {
        for (let i = 0; i < this.attack_range.length; i++) {
            if ((this.x_idx + this.attack_range[i][0]) == tar_x) {
                if ((this.y_idx + this.attack_range[i][1]) == tar_y) {
                    return true;
                }
            }
        }
        return false;
    },
    /*
    1 2 3
    8 9 4
    7 6 5
    */
    getTargetFieldIdx(node, tar_x, tar_y) {
        // function canVisit(a, b) {
        //     if (a < 0 || b < 0 || a >= node.field['row_n'] || b >= node.field['col_n']) {
        //         return false;
        //     }
        //     return true;
        // }
        let next_dir = [[-1, 0], [1, 0], [0, -1], [0, 1]];
        // for (let dir_idx = 0; dir_idx < next_dir.length; dir_idx++) {
        //     let next_x = tar_x + next_dir[dir_idx][0];
        //     let next_y = tar_y + next_dir[dir_idx][1];
        //     if (canVisit(next_x, next_y)) {
        //         return [next_x, next_y];
        //     }
        // }

        if (tar_x < node.x_idx && tar_y > node.y_idx) {
            // 1
            let next_x = tar_x + next_dir[2][0];
            let next_y = tar_y + next_dir[2][1];
            return [next_x, next_y];
        }

        if (tar_x == node.x_idx && tar_y > node.y_idx) {
            // 2
            let next_x = tar_x + next_dir[2][0];
            let next_y = tar_y + next_dir[2][1];
            return [next_x, next_y];
        }

        if (tar_x > node.x_idx && tar_y > node.y_idx) {
            // 3
            let next_x = tar_x + next_dir[2][0];
            let next_y = tar_y + next_dir[2][1];
            return [next_x, next_y];
        }
        if (tar_x > node.x_idx && tar_y == node.y_idx) {
            // 4
            let next_x = tar_x + next_dir[0][0];
            let next_y = tar_y + next_dir[0][1];
            return [next_x, next_y];
        }
        if (tar_x > node.x_idx && tar_y < node.y_idx) {
            // 5
            let next_x = tar_x + next_dir[3][0];
            let next_y = tar_y + next_dir[3][1];
            return [next_x, next_y];
        }

        if (tar_x == node.x_idx && tar_y < node.y_idx) {
            // 6
            let next_x = tar_x + next_dir[3][0];
            let next_y = tar_y + next_dir[3][1];
            return [next_x, next_y];
        }
        if (tar_x < node.x_idx && tar_y < node.y_idx) {
            // 7
            let next_x = tar_x + next_dir[3][0];
            let next_y = tar_y + next_dir[3][1];
            return [next_x, next_y];
        }
        if (tar_x < node.x_idx && tar_y == node.y_idx) {
            // 7
            let next_x = tar_x + next_dir[1][0];
            let next_y = tar_y + next_dir[1][1];
            return [next_x, next_y];
        }
        return [tar_x, tar_y];
    },

    setScreenWH(sw, sh) {
        this.screen_w = sw;
        this.screen_h = sh;
    },
    // onLoad () {},
    random(lower, upper) {

        return Math.floor(Math.random() * (upper - lower)) + lower;

    },
    loadRes(x_idx, y_idx, tar_x_idx, tar_y_idx, field) {

        var node = cc.find("Canvas/Root");
        var self = this;
        self.root = node;
        cc.resources.load('gou', cc.SpriteFrame, function (err, spriteFrame) {
            var p_node = new cc.Node('p');
            const sprite = p_node.addComponent(cc.Sprite);
            sprite.spriteFrame = spriteFrame;
            let scalex = field['bd_w'] / sprite.node.width;
            let scaley = field['bd_h'] / sprite.node.height;
            self.player_node = p_node;
            self.x_idx = x_idx;
            self.y_idx = y_idx;
            self.field = field;
            self.field[x_idx][y_idx].visit = 1;
            self.draw(p_node, scalex, scaley, x_idx, y_idx, field);
            cc.resources.load("progress", cc.SpriteFrame, function (proerr, prospriteFrame) {
                var pro_node = new cc.Node("health");
                var progressBar = pro_node.addComponent(cc.ProgressBar);
                var pro_sprite = pro_node.addComponent(cc.Sprite);
                pro_sprite.spriteFrame = prospriteFrame;
                progressBar.barSprite = pro_sprite;
                self.progressBar = progressBar;
                pro_node.y = field['bd_h'];
                pro_node.zIndex = 11;
                pro_node.parent = self.player_node;
            });
            cc.resources.load("progressbk", cc.SpriteFrame, function (proerr, prospriteFrame) {
                var bk_node = new cc.Node("bk");
                var progressBar = bk_node.addComponent(cc.ProgressBar);
                var pro_sprite = bk_node.addComponent(cc.Sprite);
                pro_sprite.spriteFrame = prospriteFrame;
                progressBar.barSprite = pro_sprite;

                bk_node.y = field['bd_h'];
                bk_node.zIndex = 10;
                bk_node.parent = self.player_node;

                Player.resCount++;
                if (Player.resCount == 2) {
                    self.bf.ready = true;
                    //self.bf.behaveTreeRoot.travel(self.bf.behaveTreeRoot);
                    cc.log("debug");
                }
            });
            // 获取对应的脚本
            // self.health_value_progress = self.root.getComponent('Progress');
            
            //Player.resCount++;
            if (Player.resCount == 2) {
                //start behavetree
                //self.bf.behaveTreeRoot.travel(self.bf.behaveTreeRoot);
                //cc.log("debug");
            }
            

            // 添加触摸事件
            self.player_node.on(cc.Node.EventType.TOUCH_START, onTouchStart, self);
            Player.receiveMsg(self);

            function onTouchStart() {
                let algo = new SearchAlgo();
                let tar_idx = self.getTargetFieldIdx(self, tar_x_idx, tar_y_idx);
                self.bf.showBlockRange(1, x_idx, y_idx);
                let find_a_way = algo.search(x_idx, y_idx, tar_idx[0], tar_idx[1], field);
                if (find_a_way) {
                    self.recursionMoveAction(self, algo.walk_steps, algo.walk_steps.length - 2);
                }
            }
        })


    },

    recursionMoveAction(role, steps, idx) {
        if (idx < 0) {
            console.log(role.m_id + " error idx " + idx);
            Player.boardcastMsg(role, GlobalData._instance.relmsgPos());
            return;
        }
        role.move_action_status = 1;
        role.field[role.x_idx][role.y_idx].visit = 0;
        role.bf.showBlockRange(0, role.x_idx, role.y_idx);
        role.bf.updateBf();
        role.move_action = cc.tween(this.player_node)
            .to(1 / role.move_speed, { position: cc.v2(steps[idx][0], steps[idx][1]), angle: 0 })
            .call(() => {
                role.x_idx = steps[idx].x;
                role.y_idx = steps[idx].y;
                // role.field[role.x_idx][role.y_idx].visit = 1;
                role.bf.showBlockRange(1, role.x_idx, role.y_idx);
                role.move_action_status = 0;
                role.bf.showAttackRange(role);
                // setTimeout(() => {
                //     role.recursionMoveAction(role, steps, idx - 1);
                // }, 1 / role.move_speed);

                Player.boardcastMsg(role, GlobalData._instance.relmsgPos());
            })
            .start()

    },
    draw(p_node, scalex, scaley, x_idx, y_idx, field) {
        p_node.scaleX = scalex;
        p_node.scaleY = scaley;
        p_node.x = field[x_idx][y_idx].x;
        p_node.y = field[x_idx][y_idx].y;
        p_node.zIndex = 9;
        p_node.parent = this.root;
    },
    update(dt) {

        var progress = self.progressBar.progress;
        cc.log(progress);
    },
});

export default Player;
