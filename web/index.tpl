<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html style="height: 100%" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
  <title>Moment Video Server - http://momentvideo.org</title>
  <link rel="icon" type="image/vnd.microsoft.icon" href="favicon.ico"/>
  <style type="text/css">
    body {
        font-family: sans-serif;
        color: #000000;
        background-color: #bbffbb;
    }

    .play-position {
        font-size: larger;
        color: #006600;
    }

    .cursor-position {
        font-size: larger;
        color: #000088;
    }

    .date-input {
        width: 2.1em;
        border: 1px solid #999999;
        padding: 4px 0px;
        text-align: right;
        font-size: medium;
        text-align: center;
    }

    .year-input {
        width: 3.1em;
    }

    .date-separator {
        padding: 1px;
        color: gray;
    }

    .go-button {
        color: white;
        background-color: #33aa33;
        padding-left: 0.5em;
        padding-right: 0.5em;
    }

    .go-button:hover {
      cursor: pointer;
    }

    .progressbar:hover {
      cursor: pointer;
    }
  </style>
  <script type="text/javascript" src="jquery.js"></script>
  <script type="text/javascript">
    function pad2 (a) {
        if (a < 10)
            return '0' + a;

        return '' + a;
    }

    function unixtimeToDate (unixtime) {
        var date = new Date (unixtime * 1000);
        var date_str = date.getFullYear() + '/'
                       + (date.getMonth() + 1) + '/'
                       + date.getDate() + ' '
                       + pad2 (date.getHours()) + ':'
                       + pad2 (date.getMinutes()) + ':'
                       + pad2 (date.getSeconds());
        return date_str;
    }

    var cursor_position_sel;
    var play_position_sel;
    var reference_pos = 0;
    var play_start_pos = 0;
    var last_time_delta = 0;

    function setCursorPosition (unixtime) {
        cursor_position_sel.html (unixtimeToDate (unixtime));
    }

    function setPlayStartPosition (unixtime) {
        play_start_pos = unixtime;
        last_time_delta = 0;
        setPlayPosition (0);
    }

    function setPlayPosition (time_delta) {
        if (isNaN (time_delta))
            time_delta = last_time_delta;
        else
            last_time_delta = time_delta;

        play_position_sel.html (unixtimeToDate (play_start_pos + time_delta));
        $("#mark-play")[0].style.left = Math.floor ($(window).width() / 2 + (play_start_pos - reference_pos + time_delta)) + 'px';
/*        $("#debug").html (time_delta + ', ' + (play_start_pos - reference_pos)); */
    }

    $(document).ready (function() {
        cursor_position_sel = $("#cursor-position");
        play_position_sel = $("#play-position");
        var mark_cursor_sel = $("#mark-cursor")[0];

        $(window).resize (function () {
            doResize ();
        });

        doResize ();

        /* TODO POST */
        $.get ("/mod_nvr/unixtime",
            {},
            function (data) {
                var unixtime = eval ('(' + data + ')');

                var date = new Date (unixtime * 1000);

                /* var unixtime = Math.floor (date.getTime() / 1000); */
                reference_pos = unixtime;
                setPlayStartPosition (unixtime);

                $("#pos-input-year").val (date.getFullYear());
                $("#pos-input-month").val (date.getMonth() + 1);
                $("#pos-input-day").val (date.getDate());
                $("#pos-input-hour").val (pad2 (date.getHours()));
                $("#pos-input-minute").val (pad2 (date.getMinutes()));
                $("#pos-input-second").val (pad2 (date.getSeconds()));

                $("#progressbar").click (function (e) {
                    var x = e.pageX - this.offsetLeft;
                    var y = e.pageY - this.offsetTop;
                    var time = Math.floor (unixtime - ($(window).width() / 2 - x));
                    setPlayStartPosition (time);
                    document ["MyPlayer"].setSource ("{{MyPlayerAutoplayUri}}", "{{MyPlayerAutoplayStreamName}}?start=" + time);
                });

                $("#progressbar").mousemove (function (e) {
                    var x = e.pageX - this.offsetLeft;
                    var y = e.pageY - this.offsetTop;
                    var time = unixtime - (Math.floor ($(window).width() / 2) - x);
                    mark_cursor_sel.style.left = x + 'px';
                    setCursorPosition (time);
                });

                $("#go-button").click (function (e) {
                    var date = new Date ($("#pos-input-year").val(),
                                         $("#pos-input-month").val() - 1,
                                         $("#pos-input-day").val(),
                                         $("#pos-input-hour").val(),
                                         $("#pos-input-minute").val(),
                                         $("#pos-input-second").val(),
                                         0);
                    var time = Math.floor (date.getTime() / 1000);
                    reference_pos = time;
                    setPlayStartPosition (time);
                    document ["MyPlayer"].setSource ("{{MyPlayerAutoplayUri}}", "{{MyPlayerAutoplayStreamName}}?start=" + time);
                });

                document ["MyPlayer"].setSource ("{{NvrLiveUri}}", "{{MyPlayerAutoplayStreamName}}");

                setInterval (function () {
                    setPlayPosition (document["MyPlayer"].getPlayheadTime());
                }, 1000);
            }
        );
    });

    function doResize () {
        $("#mark-middle")[0].style.left = Math.floor ($(window).width() / 2) + 'px';
        $("#mark-play")[0].style.left = Math.floor ($(window).width() / 2 + last_time_delta) + 'px';
    }

/*
    function debug_videoDim (w, h, s)
    {
        $("#debug").html (w + " x " + h + ", " + s);
    }
*/
  </script>
</head>
<body style="height: 100%; padding: 0; margin: 0">
  <div style="min-height: 100%">
    <div style="position: absolute; width: 100%; top: 0px; bottom: 80px; background-color: #aaffaa">
      <div style="width: 100%; height: 100%; background-color: #000000">
        <div id="flash_div" style="height: 100%">
          <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000"
                  width="100%"
                  height="100%"
                  id="MyPlayer"
                  align="middle">
            <param name="movie" value="MyPlayer.swf?1"/>
            <param name="allowScriptAccess" value="always"/>
            <param name="quality" value="high"/>
            <param name="scale" value="noscale"/>
            <param name="salign" value="lt"/>
            <param name="bgcolor" value="#000000"/>
            <param name="allowFullScreen" value="true"/>
            <param name="FlashVars" value="{{#MyPlayerAutoplay_OFF}}autoplay=0&{{/MyPlayerAutoplay_OFF}}playlist={{#MyPlayerPlaylist_ON}}1{{/MyPlayerPlaylist_ON}}{{#MyPlayerPlaylist_OFF}}0{{/MyPlayerPlaylist_OFF}}&buffer={{MyPlayerBuffer}}{{#MyPlayerAutoplayUri_ON}}&uri={{MyPlayerAutoplayUri}}&stream={{MyPlayerAutoplayStreamName}}{{/MyPlayerAutoplayUri_ON}}"/>
            <embed              FlashVars="{{#MyPlayerAutoplay_OFF}}autoplay=0&{{/MyPlayerAutoplay_OFF}}playlist={{#MyPlayerPlaylist_ON}}1{{/MyPlayerPlaylist_ON}}{{#MyPlayerPlaylist_OFF}}0{{/MyPlayerPlaylist_OFF}}&buffer={{MyPlayerBuffer}}{{#MyPlayerAutoplayUri_ON}}&uri={{MyPlayerAutoplayUri}}&stream={{MyPlayerAutoplayStreamName}}{{/MyPlayerAutoplayUri_ON}}"
                src="MyPlayer.swf?1"
                bgcolor="#000000"
                width="100%"
                height="100%"
                name="MyPlayer"
                quality="high"
                align="middle"
                scale="showall"
                allowFullScreen="true"
                allowScriptAccess="always"
                type="application/x-shockwave-flash"
                pluginspage="http://www.macromedia.com/go/getflashplayer"
            />
          </object>
        </div>
      </div>
    </div>
    <div id="progressbar" class="progressbar" style="position: absolute; overflow: hidden; width: 100%; height: 40px; bottom: 40px; background-color: #ffffbb">
      <div id="mark-middle" style="position: absolute; background-color: #33aa33; left: 0px; width: 2px; height: 40px">
      </div>
      <div id="mark-cursor" style="position: absolute; background-color: #0000ff; left: 0px; width: 2px; height: 40px">
      </div>
      <div id="mark-play" style="position: absolute; background-color: #aa3333; left: 0px; width: 2px; height: 40px">
      </div>
    </div>
    <div style="position: absolute; width: 100%; height: 40px; bottom: 0px">
      <div style="float: left; height: 100%; padding-left: 1ex">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; margin-left: auto; margin-right: auto">
          <tr>
            <td>
              <div id="play-position" class="play-position"></div>
            </td>
            <!--
            <td>
              <div id="debug" style="padding-left: 1em">DEBUG</div>
            </td>
            -->
          </tr>
        </table>
      </div>
      <div style="float: right; height: 100%">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; padding-left: 1ex; padding-right: 0px">
          <tr>
            <td>
              <input type="text" id="pos-input-year" class="date-input year-input" placeholder="Год"/>
            </td>
            <td class="date-separator">/</td>
            <td>
              <input type="text" id="pos-input-month" class="date-input" placeholder="Мес"/>
            </td>
            <td class="date-separator">/</td>
            <td>
              <input type="text" id="pos-input-day" class="date-input" placeholder="Ден"/>
            </td>
            <td>&nbsp;&nbsp;</td>
            <td>
              <input type="text" id="pos-input-hour" class="date-input" placeholder="Час"/>
            </td>
            <td class="date-separator">:</td>
            <td>
              <input type="text" id="pos-input-minute" class="date-input" placeholder="Мин"/>
            </td>
            <td class="date-separator">:</td>
            <td style="padding-right: 0px">
              <input type="text" id="pos-input-second" class="date-input" placeholder="Сек"/>
            </td>
            <td style="padding-left: 5px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="go-button" class="go-button">
                    Переход
                  </td>
                </tr>
              </table>
            </td>
          </tr>
        </table>
      </div>
      <div style="float: right; height: 100%; padding-left: 1ex; padding-right: 1ex">
          <table border="0" cellpadding="0" cellspacing="0" style="height: 100%">
            <tr>
              <td>
                <div id="cursor-position" class="cursor-position"></div>
              </td>
            </tr>
          </table>
      </div>
    </div>
  </div>
</body>
</html>

