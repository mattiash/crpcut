#!/bin/bash


tests=(
    "           asserts"          "run=25 failed=14"
    "-v         asserts"          "run=25 failed=14 verbose=1"
    "-c 8       asserts"          "run=25 failed=14"
    "-c 8 -v    asserts"          "run=25 failed=14 verbose=1"

    "-d         asserts"          "run=25 failed=14 nodeps=1"
    "-d -v      asserts"          "run=25 failed=14 nodeps=1 verbose=1"
    "-d -c 8    asserts"          "run=25 failed=14 nodens=1"
    "-d -c 8 -v asserts"          "run=25 failed=14 nodeps=1 verbose=1"

    "           asserts timeouts" "run=31 failed=18"
    "-v         asserts timeouts" "run=31 failed=18 verbose=1"
    "-c 8       asserts timeouts" "run=31 failed=18"
    "-c 8 -v    asserts timeouts" "run=31 failed=18 verbose=1"

    "-d         asserts timeouts" "run=31 failed=18 nodeps=1"
    "-d -v      asserts timeouts" "run=31 failed=18 nodeps=1 verbose=1"
    "-d -c 8    asserts timeouts" "run=31 failed=18 nodeps=1"
    "-d -c 8 -v asserts timeouts" "run=31 failed=18 nodeps=1 verbose=1"

    ""           "run=61 failed=36 blocked=2"
    "-v"         "run=61 failed=36 blocked=2 verbose=1 "
    "-c 8"       "run=61 failed=36 blocked=2"
    "-c 8 -v"    "run=61 failed=36 blocked=2 verbose=1"

    "-d"         "run=63 failed=36 blocked=0 nodeps=1"
    "-d -v"      "run=63 failed=36 blocked=0 nodeps=1 verbose=1"
    "-d -c 8"    "run=63 failed=36 blocked=0 nodeps=1"
    "-d -c 8 -v" "run=63 failed=36 blocked=0 nodeps=1 verbose=1"
    )

n=0
while [ $n -lt ${#tests[*]} ]
do
    param="${tests[$n]}"
    expect="${tests[$(($n+1))]}"
    printf "testrun %-32s: " "$param"
    filename=/tmp/crpcut_sanity$$_$(($n/2+1)).xml
    reportfile=/tmp/crpcut_sanity_report$$_$(($n/2+1))
    ./test/testprog $param > $filename
    rv=$?
    xmllint --noout --schema crpcut.xsd $filename 2> /dev/null || {
        echo "$filename violates crpcut.xsd XML Schema"
        return 1
    }
    r=()
    lineno=0
    ./filter.awk -- registered=63 rv=$rv $expect < $filename > $reportfile
    [ $? == 0 ] || {
        echo FAILED
        cat $reportfile
        rm $reportfile
        echo "The test report is in $filename"
        exit 1
    }
    echo "OK"
    rm $filename
    rm $reportfile
    n=$(($n+2))
done
