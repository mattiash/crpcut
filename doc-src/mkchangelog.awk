#!/bin/awk -f

BEGIN {
    print "<html><head>"
    print "<title>crpcut ChangeLog</title>"
    print "<link rel=\"stylesheet\" href=\"homepage-" version ".css\""
    print "type=\"text/css\"></head>"
    print "<body><h4><span class=\"productname\">crpcut</span> ChangeLog</h4>"
    print "<table>";
}
/^[[:digit:]]/ {
    if (in_bullet)
    {
        print "</li>";
        in_bullet = 0;
        paragraph=0;
    }
    if (in_row)
    {
        print "</ul></td></tr>";
    }
    print "<tr><td>" $1 "</td><td width=\"10px\"></td><td><b>";
    gsub(/^[^[:space:]]+/, "");
    gsub(/http:[^[:space:],\)]*/, "<a href=\"&\">&</a>");
    print $0 "</b></td></tr><tr><td></td><td></td><td><ul>";
    in_row=1;
    next;
}
/^[[:space:]]*\*/ {
    if (in_bullet)
    {
        print "</li>";
        in_bullet = 0;
    }
    paragraph = 0;
    gsub(/^[[:space:]]*[^[:space:]]+/, "");
    gsub(/http:[^[:space:],\)]*/, "<a href=\"&\">&</a>");
    print "<li>" $0
    in_bullet = 1;
    next;
}
/^[[:space:]]*$/ {
    paragraph=1;
    next;
}
 {
     if (paragraph)
     {
         print "<p/>";
         paragraph=0;
     }
     gsub(/http:[^[:space:],\)]*/, "<a href=\"&\">&</a>");
     print $0;
 }
 END {
     if (in_bullet)
     {
         print "</li>"
     }
     print "</ul></td></tr></table></body></html>";
 }
