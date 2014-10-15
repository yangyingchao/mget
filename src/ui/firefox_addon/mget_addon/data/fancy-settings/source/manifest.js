// Edit with Emacs options
this.manifest = {
    "name": "mget downloader",
    "icon": "../../icons/down.png",
    "settings": [
        {
            "tab": i18n.get("activities"),
            "group": i18n.get("ongoing"),
            "name": "Description",
            "type": "description",
            "text": i18n.get("active_downloads")
        },
        {
            "tab": i18n.get("activities"),
            "group": i18n.get("finished"),
            "name": "Description",
            "type": "description",
            "text": i18n.get("finished_downloads")
        },
        {
            "tab": "Configuration",
            "group": "Edit Server",
            "name": "edit_server_host",
            "type": "text",
            "label": "Edit Server Host:",
        },
        {
            "tab": "Configuration",
            "group": "Edit Server",
            "name": "edit_server_port",
            "type": "text",
            "label": "Edit Server Port:",
        },
	    {
            "tab": "Configuration",
            "group": "Interface",
	        "name": "enable_button",
	        "type": "checkbox",
	        "label": "Show 'edit' button next to textarea"
	    },
	    {
            "tab": "Configuration",
            "group": "Interface",
	        "name": "enable_contextmenu",
	        "type": "checkbox",
	        "label": "Enable context menu item to invoke editor"
	    },
	    {
            "tab": "Configuration",
            "group": "Interface",
	        "name": "enable_dblclick",
	        "type": "checkbox",
	        "label": "Allow double click on textarea to invoke editor"
	    },
	    {
            "tab": "Configuration",
            "group": "Interface",
	        "name": "enable_keys",
	        "type": "checkbox",
	        "label": "Enable Alt-Enter Keyboard shortcut to invoke editor"
	    },
        {
            "tab": "Test",
            "group": "Test",
	        "name": "TestButton",
            "type": "button",
            "label": i18n.get("Test Edit Server"),
	        "text": i18n.get("Test")
        },
        {
            "tab": "Test",
            "group": "Test",
            "name": "enable_debug",
            "type": "checkbox",
            "label": i18n.get("enable_debug")
        }
    ]
};
