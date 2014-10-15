// const Preferences = require("preferences");
// const Version = require("version");
// const {defer} = require("support/defer");
// const Mediator = require("support/mediator");
// const DTA = require("api");
// const Utils = require("utils");
// const obs = require("support/observers");
const {Frame} = require("sdk/ui/frame");
const Buttons = require('sdk/ui/button/action');
const Tabs = require("sdk/tabs");
var {Toolbar} = require("sdk/ui/toolbar");
var { ToggleButton } = require("sdk/ui");
var { data } = require("sdk/self");

var popup_panel = require("sdk/panel").Panel({
    width: 400,
    height: 320,
    contentURL: data.url("fancy-settings/source/popup.html"),
    contentScriptFile: [data.url("jquery-1.4.4.min.js"),
                        data.url("panel.js")],
    onHide: handleHide
});

var button = ToggleButton({
    id : "mozilla-link",
    label: "Mget Added",
    icon: "./down.png",
    onChange: handleChange
});


var contextMenu = require("sdk/context-menu");
var menuItem = contextMenu.Item({
    label: "Log Selection",
    context: contextMenu.URLContext("*"),
    contentScript: 'self.on("click", function () {' +
        '  var text = window.getSelection().toString();' +
        '  self.postMessage(text);' +
        '});',
    onMessage: function (selectionText) {
        console.log(selectionText);
    }
});


// var panel = require("sdk/panel").Panel({
//     contentURL: "http://www.baidu.com",
//     width: 400,
//     height: 320,
//     contentScript: myScript
// });

// panel.port.on("click-link", function(url) {
//   console.log(url);
// });

function handleChange (state)
{
    if (state.checked) {
        popup_panel.show({ position: button });
    }
    else {
        console.log('tab is loaded', state);
    }

    console.log('tab is loaded', state);
}

function handleHide() {
    button.state('window', { checked: false });
}
