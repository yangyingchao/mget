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


var reddit_panel = require("sdk/panel").Panel({
  width: 400,
  height: 320,
  contentURL: require("sdk/self").data.url("index.html"),
  // contentScriptFile: [data.url("jquery-1.4.4.min.js"),
  //                     data.url("panel.js")],
  onHide: handleHide
});

let button = ToggleButton({
    id : "mozilla-link",
    label: "Visit Mozilla",
    icon: "./icon-32.png",
    onChange: handleChange
});


function handleChange (state)
{
    if (state.checked) {
        reddit_panel.show({ position: button });
    }
    else {
        console.log('tab is loaded', state);
    }

    console.log('tab is loaded', state);
    // Tabs.open("http://www.baidu.com");


    var frame = new Frame({
        url: "./index.html",
        onAttach: () => {
            console.log("frame was attached");
        },
        onReady: () => {
            console.log("frame document was loaded");
        },
        onLoad: () => {
            console.log("frame load complete");
        },
        onMessage: (event) => {
            console.log("got message from frame content", event);
            if (event.data === "ping!")
                event.source.postMessage("pong!", event.source.origin);
        }
    });

    var toolbar =  Toolbar ({
        name: "city-info",
        title: "City Info",
        items:[frame]
    });


    frame.postMessage({
        hello: "content"
    });
}

function handleHide() {
  button.state('window', { checked: false });
}
