import {Queue} from './Queue';
export class SearchAlgo {
    constructor() {
        this.walk_steps = new Array();
    }

    search = function (a, b, c, d, field) {
        let que = new Queue();
        let start_pos = [a, b];
        function canVisit(a, b) {
            if (a < 0 || b < 0 || a >= field['row_n'] || b >= field['col_n']) {
                return false;
            }

            return [true, que];
        }
        if (canVisit(a, b) == false || canVisit(c, d) == false) {
            return [false, que];
        }
        
        var shortest_way = new Array();
        for (let i = 0; i < field['row_n']; i++) {
            shortest_way[i] = new Array();
            for (let j = 0; j < field['col_n']; j++) {
                shortest_way[i][j] = {};
                shortest_way[i][j].visit = field[i][j].visit;
            }
        }

        que.enqueue(start_pos);
        let find = false;
        while (!que.isEmpty()) {
            let now_pos = que.front();
            if (now_pos[0] == c && now_pos[1] == d) {
                find = true;
                break;
            }
            que.dequeue();
            let next_dir = [[-1, 0], [1, 0], [0, -1], [0, 1]];
            for (let dir_idx = 0; dir_idx < next_dir.length; dir_idx++) {
                let next_x = now_pos[0] + next_dir[dir_idx][0];
                let next_y = now_pos[1] + next_dir[dir_idx][1];
                // cc.log(next_x + ',' + next_y);
                if (canVisit(next_x, next_y) && shortest_way[next_x][next_y].visit == 0) {
                    shortest_way[next_x][next_y].pre = now_pos;
                    shortest_way[next_x][next_y].visit = 1;
                    que.enqueue([next_x, next_y]);
                }
            }
        }

        if (find) {
            let tar_x = c;
            let tar_y = d;
            let loop = 0;
            let pre_pos = shortest_way[tar_x][tar_y].pre;
            this.walk_steps[loop] = field[c][d].pos;
            this.walk_steps[loop].x = c;
            this.walk_steps[loop].y = d;
            loop++;
            while (pre_pos != undefined) {
                // cc.log(pre_pos[0], pre_pos[1]);
                pre_pos = shortest_way[pre_pos[0]][pre_pos[1]].pre;
                if (pre_pos == undefined) {
                    break;
                }
                this.walk_steps[loop] = field[pre_pos[0]][pre_pos[1]].pos;
                this.walk_steps[loop].x = pre_pos[0];
                this.walk_steps[loop].y = pre_pos[1];
                loop++;
                if (pre_pos[0] == start_pos[0] && pre_pos[1] == start_pos[1]) {
                    break;
                }
            }
        }
        // que.print();
        return [find, que];
    };
};
