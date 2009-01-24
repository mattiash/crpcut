#!/usr/bin/awk

/<test name=.*should_fail.*result=\"FAILED\">/,/<\/test>/ {
    if ($0 ~ /<test name/) ++failures; next;
}
/<test name=.*should_fail.*result=\"OK\"/ {
    unexpected_success[failed_ok++]=$0; next;
}
/<test name=.*succ.*FAILED/ {
    unexpected_fail[ok_fail++]=$0; next;
}
/<test name=.*succ.*OK\"\/>/ { ++succeeded; next;}
/<test name=.*succ.*OK\">/,/<\/test>/ { if ($0 ~ /<test name/) ++succeeded; next;}
/<registered_test_cases>/ {
    e_registered=gensub(/<\/?[a-z_]*>/, "", "g") + 0; next;
}
/<run_test_cases>/ {
    e_run=gensub(/<\/?[a-z_]*>/, "", "g") + 0; next;
}
/<failed_test_cases>/ {
    e_failed=gensub(/<\/?[a-z_]*>/, "", "g") + 0; next;
}
{ print $0 }
END {
    report=0;
    if (e_failed != failures) {
        print "expected ", e_failed, "failures whereas ", failures, "were detected"
        report=1;
    }
    if (e_run - e_failed != succeeded) {
        print "expected ", e_run - e_failed, "successful tests, but found", succeeded;
        report=1;
    }
    if (failed_ok) {
        report=1;
        print "The following tests shouldn't have succeeded:";
        for (a in unexpected_success)
        {
            reason=""
            line=unexpected_success[a]
            if (line ~ /core/) {
                reason="\n     (is your ulimit setting preventing core dumps?)"
            }
            print line reason
        }
    }
    if (ok_failed) {
        report=1;
        print "The following tests shouldn't have failed:"
        for (a in unexpected_fail)
        {
            print unexpected_fail[a];
        }
    }
    if (!report)
    {
        print "Looks OK";
    }
}
