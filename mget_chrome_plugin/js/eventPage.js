
// The onClicked callback function.
function onClickHandler(info, tab) {
    // console.log("item " + info.menuItemId + " was clicked");
    console.log("info: " + JSON.stringify(info));
    // console.log("tab: " + JSON.stringify(tab));

    var url = info.linkUrl;
    console.log("Url: " + url);

    // TODO: it is not allowed to launch native apps in extensions, needs to
    //       use NPAPI plugin which is not encouraged.
    //       I'll need to update mget to start it as server-mode, then listen
    //       to local sockets ...
};

chrome.contextMenus.onClicked.addListener(onClickHandler);

// Set up context menu tree at install time.
chrome.runtime.onInstalled.addListener(function() {
  // Create one test item for each context type.
  var contexts = ["page","link","image","video", "audio"];
  for (var i = 0; i < contexts.length; i++) {
    var context = contexts[i];
    var title = "Download " + context + " with mget.";
    var id = chrome.contextMenus.create({"title": title, "contexts":[context],
                                         "id": "context" + context});
    console.log("'" + context + "' item:" + id);
  }
});
