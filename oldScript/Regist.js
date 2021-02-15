// Learn cc.Class:
//  - https://docs.cocos.com/creator/manual/en/scripting/class.html
// Learn Attribute:
//  - https://docs.cocos.com/creator/manual/en/scripting/reference/attributes.html
// Learn life-cycle callbacks:
//  - https://docs.cocos.com/creator/manual/en/scripting/life-cycle-callbacks.html

cc.Class({
    extends: cc.Component,

    properties: {
        button: cc.Button
    },

    onLoad: function () {
        var node = cc.find("Canvas/regist");
        this.button = node;
        var l = cc.resources.load("xx");
        this.button.on('click', this.callback, this);
    },

    request: function (obj) {
        var httpRequest = new XMLHttpRequest();
        var time = 5 * 1000;
        var timeout = false;

        // 超时设置
        var timer = setTimeout(function () {
            timeout = true;
            httpRequest.abort();
        }, time);

        var url = obj.url;

        // 组织请求参数
        if (typeof obj.data == 'object') {
            console.info('obj.data=' + JSON.stringify(obj.data));
            var kvs = []
            for (var k in obj.data) {
                kvs.push(encodeURIComponent(k) + '=' + encodeURIComponent(obj.data[k]));
            }
            url += '?';
            url += kvs.join('&');
        }

        httpRequest.open(obj.method ? obj.method : 'GET', url, true);

        httpRequest.onreadystatechange = function () {
            var response = httpRequest.responseText;
            console.info('http url cb:' +  url + ' readyState:' + httpRequest.readyState + ' status:' + httpRequest.status);
            clearTimeout(timer);

            if (httpRequest.readyState == 4) {
                console.info('http success:' + url + ' resp:' + response);
             //   var resJson = JSON.parse(response);
             //   if (typeof obj.success == 'function') {
             //       obj.success(resJson);
             //   }
            } else {
                console.info('http fail:' + url);
                if (typeof obj.fail == 'function') {
                    obj.fail(response);
                }
            }
        };
        httpRequest.send();
    },

    callback: function (button) {
        var obj = {
            'url' : 'http://127.0.0.1:3000/',
            'success' : function(jsonData) {
                this.responstData.string = jsonData['info'];
            }.bind(this)
        }
        this.request(obj);
    }
});
