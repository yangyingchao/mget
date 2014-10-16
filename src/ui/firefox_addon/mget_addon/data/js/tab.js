var t;


$(document).ready(function(){
    $("p").click(function(){
        $(this).hide();
    });

    $(".tab").click(function () {
        var label = $(this).attr('id');
        if ($(this).attr('class') != "tab active"){
            var active_tab = $(".tab.active");
            var active_content = $(active_tab).text();
            var name = $(this).text();
            active_tab.attr('class', 'tab');
            $(this).attr('class', 'tab active');

            var eles = $("#content").children();

            var i = eles.length;
            while( i -- ){
                var ele = eles[i];
                console.log(i + ": class: " + $(ele).attr('class') +
                            ", id: " + $(ele).attr('id') +", label: "+label
                           );
                if ($(ele).attr('id') == label) {
                    $(ele).attr('class', 'tab-content show');
                }
                else {
                    $(ele).attr('class', 'tab-content');
                }
            }
        }
    });

    function timedCount ()
    {
        $("#p1").val(Math.random()*80);
        t = setTimeout(timedCount, 1000)
    }

    $("#update_btn").click(function(){
        timedCount();
    });


    // window.setInterval(timedCount, 2000);

});
