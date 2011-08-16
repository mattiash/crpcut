/*jslint browser: true, white: true */

/*global $ */


$(document).ready(
  function() {
    "use strict";
    var url = window.location.search.substr(1);

    // Dummy function to mark that a parameter is unused.
    // Silences warnings from jslint.
    function unused() {}

    function marker( cl ) {
      if( cl === "violation" ) {
        return "(v) ";
      }
      else if( cl === "stdout" ) {
        return "(o) ";
      }
      else if( cl === "stderr" ) {
        return "(e) ";
      }
      else if( cl === "info" ) {
        return "(i) ";
      }
      else {
        return "??? ";
      }
    }

    function markup( text, cl ) {
      var m = "\n" + marker(cl);
      text = $.trim(text);
      return marker(cl) + text.replace( /\n/g, m );
    }

    $("#error").ajaxError(
      function(event, request, settings){
        unused(event, request);
        $(this).append("Failed to fetch <a href='" + settings.url + "'>" +
                       settings.url + "</a>");
      });

    $.get(url,
          function(d){
            var identifier, program, html, registered, run, failed;

            identifier = $(d).find('crpcut').attr('id');
            if( !identifier ) {
              identifier="";
            }

            program = url.match( /([\-a-zA-Z0-9_]+)\.xml/ );

            html = "";
            html += '<h1 id="qunit-header">' + program[1] + '</h1>';
            html += '<h2 id="qunit-banner" class="pass"></h2>';
            html += '<h2 id="qunit-userAgent">' + identifier + '</h2>';
            html += '<ol id="qunit-tests">';

            if( $(d).find('crpcut').length === 0 ) {
              $('#error').append(
                "Failed to parse <a href='" + url + "'>" + url + "</a>");
              return;
            }

            $(d).find('crpcut > test').each(
              function(){
                var $test = $(this),
                name = $test.attr("name"),
                result = $test.attr("result"),
                cl = "unknown",
                log = "";

                if( result === "PASSED" ) {
                  cl = "pass";
                }
                else if( result === "FAILED" ) {
                  cl = "fail";
                }

                html += '<li class="' + cl + '">';

                $test.find('log *').each(
                  function(){
                    var cl = this.tagName;
                    log += '<li class="' + cl + '">';
                    log += markup($(this).text(), cl);
                    log += '</li>';
                  });

                if(log !== "") {
                  html += "<a class='expanditem test-name' href=''>" + name
                  + " (more)</a><ul class='log'>"+log+"</ul>";
                }
                else {
                  html += '<span class="test-name">' + name + '</span>';
                }

                html += '</li>';
              });


            $(d).find('blocked_tests > test').each(
              function(){
                var $test = $(this),
                name = $test.attr("name"),
                cl = "block";

                html += '<li class="' + cl + '">';
                html += '<span class="test-name">(blocked) ' + name
                  + '</span>';
                html += '</li>';
              });

            html += '</ol>';

            registered = $(d).find("registered_test_cases").text();
            run = $(d).find("run_test_cases").text();
            failed = $(d).find("failed_test_cases").text();

            html += "<p id='qunit-testresult' class='result'>";
            html += "Ran " + run + " tests of " + registered + ", " +
              failed + " failed. ";
            html += "<br>Generated from <a href='" + url + "'>xml</a>.";
            html += "</p>";
            $('body').append($(html));

            $("#qunit-banner").addClass(
              failed === 0 ? "qunit-pass" : "qunit-fail" );

            document.title = program[1] + " " + identifier + " test result";

            $(".log").hide();
            $(".expanditem").click(
              function() {
                $(this).siblings(".log").slideToggle( "fast" ); return false;
              } );
          });
  });

