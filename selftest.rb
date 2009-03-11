#!/usr/bin/ruby

#  Copyright 2009 Bjorn Fahller <bjorn@fahller.se>
#  All rights reserved
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.

#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.

require "rexml/document"

class Test
  def initialize(result)
    @result = result
    @log = {}
    @files = []
  end
  def expected_result?(re)
    @result =~ re
  end
  def log(type, re)
    if @log.has_key? type then
      @log[type].push re
    else
      @log[type] = [ re ]
    end
    self
  end
  def file(name)
    @files.push name
    self
  end
  def result_of(test)
    result = test.attributes['result']
    return "#{result} when #{@result} expected" if @result != result
    log = @log.clone
    dirname = nil
    test.elements.each('log/*') do |entry|
      text = entry.text || ""
      name = entry.name
      t = log[name]
      return "#{name} unexpected" if !t
      t = t.clone
      if name == 'violation' then
        dirname = entry.attributes['nonempty_dir']
        if dirname then
          begin
            isdir = dirname && File.stat(dirname).directory?
          rescue
          end
          return "#{dirname} is not a directory" if !isdir
        end
      end
      re = t.delete_at(0)
      return "Too many #{name}'s" if !re
      return "#{text} doesn't match #{name}" if !re.match(text)
    end
    return "#{dirname} has unexpected files" if dirname && @files.empty?
    return "#{@files} is missing" if !dirname && !@files.empty?
    @files.each() do | name |
      path=dirname + '/' + name
      is_found=nil
      begin
        is_found = File.stat(path)
      rescue
      end
      return "#{name} is missing" if !is_found
      File::unlink(path)
    end
    begin
      dirname && Dir::rmdir(dirname)
    rescue
      return "working dir has unexpected files"
    end
    result
  end
end

A_H='asserts_and_depends\.cpp:\d+\s+'
P_H='parametrized\.cpp:\d+\s+'
A_T='Actual time to completion was'
S_E='std::exception\s+what\(\)'
R_E='std::range_error'
TESTS = {
  'asserts::should_fail_assert_exception_with_wrong_exception' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly caught std::exception\s+what\(\)=/me),

  'asserts::should_fail_assert_no_throw_with_std_exception_string_apa' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_NO_THROW\(throw #{R_E}\("apa"\)\)\s+caught #{S_E}=apa/me),

  'asserts::should_fail_assert_no_throw_with_unknown_exception' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_NO_THROW\(\s*throw\s+1\s*\)\s+caught\s+\.\.\./me),

  'asserts::should_fail_assert_throw_any_with_no_exception' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_THROW\(i=1, \.\.\.\)\s*Did not throw/me),

  'asserts::should_fail_assert_throw_with_no_exception' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_THROW\(i=1, std::exception\)\s+Did not throw/me),

  'asserts::should_fail_on_assert_eq_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_EQ\(num, 3\)\s+where\s+num\s*=\s*4/me),

  'asserts::should_fail_on_assert_false_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_FALSE\(num\)\s+where\s+num\s*=\s*3/me),

  'asserts::should_fail_on_assert_ge_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_GE\(num, 3\)\s+where\s+num\s*=\s*2/me),

  'asserts::should_fail_on_assert_gt_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_GT\(num, 3\)\s+where\s+num\s*=\s+3/me),

  'asserts::should_fail_on_assert_gt_with_unstreamable_param_i' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_GT\(i, num\)\s+where\s+i\s*=\s*\d+-byte object <[03 ]+>\s+num\s*=\s*3/me),

  'asserts::should_fail_on_assert_le_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_LE\(num, 3\)\s+where\s+num\s*=\s*4/me),

  'asserts::should_fail_on_assert_lt_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_LT\(num, 3\)\s+where\s+num\s*=\s*3/me),

  'asserts::should_fail_on_assert_ne_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_NE\(num, 3\)\s+where\s+num\s*=\s*3/me),

  'asserts::should_fail_on_assert_true_with_fixture' =>
  Test.new('FAILED').
  log('violation',
      /#{A_H}ASSERT_TRUE\(num\)\s+where\s+num\s*=\s*0/me),

  'asserts::should_succeed_assert_no_throw' =>
  Test.new('OK'),

  'asserts::should_succeed_assert_throw_with_correct_exception' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_eq_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_false_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_ge_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_gt_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_le_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_lt_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_ne_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_on_assert_true_with_fixture' =>
  Test.new('OK'),

  'asserts::should_succeed_throw_any_with_int_exception' =>
  Test.new('OK'),

  'death::by_exception::should_fail_any_exception' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly did not throw/),

  'death::by_exception::should_fail_due_to_std_exception_with_string_apa' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly caught #{S_E}=apa/me),

  'death::by_exception::should_fail_due_to_unknown_exception' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly caught \.\.\./),

  'death::by_exception::should_fail_with_no_exception' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly did not throw/),

  'death::by_exception::should_fail_with_wrong_exception' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly caught \.\.\./),

  'death::by_exception::should_succed_with_any_exception' =>
  Test.new('OK'),

  'death::by_exception::should_succeed_with_range_error_thrown' =>
  Test.new('OK'),

  'death::by_exit::should_fail_with_exit_code_3' =>
  Test.new('FAILED').
  log('violation',
      /Exited with code 3\s+Expected normal exit/me),

  'death::by_exit::should_fail_with_no_exit' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly survived\s+Expected exit with code 3/me),

  'death::by_exit::should_fail_with_wrong_exit_code' =>
  Test.new('FAILED').
  log('violation',
      /Exited with code 4\s+Expected exit with code 3/me),

  'death::by_exit::should_succeed_with_exit_code_3' =>
  Test.new('OK'),

  'death::by_signal::should_fail_with_left_behind_core_dump_due_to_death_on_signal_11' =>
  Test.new('FAILED').
  log('violation',
      /Died with core dump/).
  file('core'),

  'death::by_signal::should_fail_with_normal_exit' =>
  Test.new('FAILED').
  log('violation',
      /Unexpectedly survived\s+Expected signal 11/me),

  'death::by_signal::should_fail_with_wrong_signal' =>
  Test.new('FAILED').
  log('violation',
      /Died on signal 6\s+Expected signal 11/me),

  'death::by_signal::should_fail_without_core_dump_with_death_on_signal_11' =>
  Test.new('FAILED').
  log('violation',
      /Died on signal 11\s+Expected normal exit/me),

  'death::by_signal::should_succeed_with_death_on_signal_11' =>
  Test.new('OK'),

  'default_success' =>
  Test.new('OK'),

 'depends::should_not_run_due_to_one_failed_dependency_success_otherwise' =>
  Test.new('OK'),

  'depends::should_succeed_after_success_dependencies' =>
  Test.new('OK'),

  'ext_parameters::should_succeed_expected_value' =>
  Test.new('OK').
  log('info',
      /katt/),

  'ext_parameters::should_succeed_no_value' =>
  Test.new('OK'),

  'ext_parameters::should_succeed_no_value_with_too_long_name' =>
  Test.new('OK'),

  'ext_parameters::should_succeed_value_interpret' =>
  Test.new('OK'),

  'ext_parameters::should_fail_value_interpret' =>
  Test.new('FAILED').
  log('violation',
      /Parameter apa with value "katt" cannot be interpreted/),

  'ext_parameters::should_fail_no_value_interpret' =>
  Test.new('FAILED').
  log('violation',
      /Parameter orm with no value cannot be interpreted/),

  'parametrized::should_fail_assert_lt_char_array_string' =>
  Test.new('FAILED').
  log('violation',
      /#{P_H}ASSERT_LT\(p1, p2\)\s+where p1 = orm\s+p2 = katt/me),

  'parametrized::should_fail_assert_lt_int_char' =>
  Test.new('FAILED').
  log('violation',
      /#{P_H}ASSERT_LT\(p1, p2\)\s+where p1 = 800\s+p2 = A/me),

  'parametrized::should_fail_assert_lt_int_double' =>
  Test.new('FAILED').
  log('violation',
      /#{P_H}ASSERT_LT\(p1, p2\)\s+where p1 = 4\s+p2 = 3.14[12]\d*/me),

  'parametrized::should_succeed_assert_lt_char_array_string' =>
  Test.new('OK'),

  'parametrized::should_succeed_assert_lt_int_char' =>
  Test.new('OK'),

  'parametrized::should_succeed_assert_lt_int_double' =>
  Test.new('OK'),

  'should_fail_after_delay' =>
  Test.new('FAILED').
  log('violation',
      /Exited with code 1\s+Expected normal exit/me),

  'should_fail_due_to_left_behind_files' =>
  Test.new('FAILED').
  log('violation', /$/e).
  file("apa"),

  'should_succeed_reading_file_in_start_dir' =>
  Test.new('OK').
  log('info', /in.rdstate\(\)=(0x)?0+/),

 'should_not_run_due_to_failed_left_behind_files_success_otherwise' =>
  Test.new('OK'),

  'output::should_fail_with_terminate' =>
  Test.new('FAILED').
  log('violation',
      /output.cpp:\d+\n\s*apa=(0[Xx])?1[fF]/),

  'output::should_succeed_with_info' =>
  Test.new('OK').
  log('info',
      /apa=3/),

  'output::should_fail_with_info' =>
  Test.new('FAILED').
  log('info',
      /apa=3/).
  log('violation',
      /Exited with code 1\s+Expected normal exit/me),

  'output::should_fail_with_death_and_left_behind_core_dump' =>
  Test.new('FAILED').
  log('stderr',
      /output\.cpp:\d+.*[Aa]ssert/me).
  log('violation',
      /Died with core dump/).
  file('core'),

  'output::should_fail_with_death_due_to_assert_on_stderr' =>
  Test.new('FAILED').
  log('stderr',
      /output\.cpp:\d+.*[Aa]ssert/me).
  log('violation',
      /Died on signal \d+\s+Expected normal exit/me),

  'output::should_succeed_with_stderr' =>
  Test.new('OK').
  log('stderr', /hello/),

  'output::should_succeed_with_stdout' =>
  Test.new('OK').
  log('stdout', /hello/),

  'timeouts::should_fail_slow_cputime_deadline' =>
  Test.new('FAILED').
  log('info', /.*/me).
  log('violation',
      /CPU-time timeout 500ms exceeded.\s+#{A_T} (([6-9]\d\d)|(1\d\d\d))ms/me),

  'timeouts::should_fail_slow_cputime_deadline_by_death' =>
  Test.new('FAILED').
  log('violation',
      /Died on signal \d+\s+Expected normal exit/me),

  'timeouts::should_fail_slow_realtime_deadline' =>
  Test.new('FAILED').
  log('violation',
      /Realtime timeout 100ms exceeded\.\s+#{A_T} [2-9]\d\dms/me),

  'timeouts::should_fail_slow_realtime_deadline_by_death' =>
  Test.new('FAILED').
  log('violation',
      /Timed out - killed/),

  'timeouts::should_succeed_slow_cputime_deadline' =>
  Test.new('OK'),

  'timeouts::should_succeed_slow_realtime_deadline' =>
  Test.new('OK'),

  'very_slow_success' =>
  Test.new('OK'),

  'wrapped::should_succeed_in_range' =>
  Test.new('OK').
  log('info',
      /d=0.523\d+/),

  'wrapped::should_fail_assert_lt' =>
  Test.new('FAILED').
  log('violation',
      /wrapped\.cpp:\d+\s+ASSERT_LT\(d, 1\.\d*\)\s+where d = 1\.(1|0999).*/me)
 }

GMOCK_TESTS = {
  'google_mock::basic_success' =>
  Test.new('OK'),

  'google_mock::should_fail_by_calling_with_wrong_value' =>
  Test.new('FAILED').
  log('violation',
      /mock function call.*call: func\(4\).*equal to 3\s+Actual: 4/me),

  'google_mock::should_fail_by_calling_too_often' =>
  Test.new('FAILED').
  log('violation',
      /more times than expected.*func\(3\)/me),

  'google_mock::should_fail_by_not_calling' =>
  Test.new('FAILED').
  log('violation',
      /call count doesn't match this expectation.*Actual: never called/me),

  'google_mock::sequence_success_1' =>
  Test.new('OK'),

  'google_mock::sequence_success_2' =>
  Test.new('OK'),

  'google_mock::sequence_should_fail_incomplete' =>
  Test.new('FAILED').
  log('violation',
      /call count doesn't match this expectation.*Actual: never called/me),

  'google_mock::sequence_should_fail_one_too_many' =>
  Test.new('FAILED').
  log('violation',
      /called more times than expected.*Actual: called twice/me),

  'google_mock::sequence_should_fail_one_wrong_value' =>
  Test.new('FAILED').
  log('violation',
      /Unexpected mock function call.*call: func\(4\).*none matched:/me),

  'google_mock::success_with_unstreamable_type' =>
  Test.new('OK'),

  'google_mock::should_fail_with_unstreamable_type_wrong_value' =>
  Test.new('FAILED').
  log('violation',
      /Unexpected mock.*Expected.*object <[03 ]*>.*Actual.*object <[04 ]*>/me)
}
tests=TESTS;
tests.merge! GMOCK_TESTS if ARGV[0] == 'gmock'
fails = 0
oks = 0
tests.each do |key, val|
  if val.expected_result?(/OK/) then
    oks+=1
  else
    fails+= 1
  end
end

BLOCKED_TESTS = [
 'depends::should_not_run_due_to_one_failed_dependency_success_otherwise',
 'should_not_run_due_to_failed_left_behind_files_success_otherwise'
]

def check_file(file_name, tests, names, results, run_count, fail_count, success_count, blocked)
  collection = {}
  report = false
  file = open("|#{file_name}")
  s = file.read
  file.close
  doc = REXML::Document.new s
  rc = $?.exitstatus
  if rc != fail_count then
    puts "\lwrong retcode #{rc}, expecting #{fail_count}"
    report = true
  end
  doc.elements.each('crpcut/test') do |e|
    name = e.attributes['name'];
    t = tests[name]
    if !t then
      r = "unknown test case"
    elsif blocked.include?(name) || name !~ names || !t.expected_result?(results) then
      r = "shouldn't have run"
    else
      r = t.result_of(e)
    end
    collection[r] = [] if !collection[r]
    collection[r].push name
  end
  stats = doc.elements['crpcut/statistics']
  regs = stats.elements['registered_test_cases'].text.to_i
  runs = stats.elements['run_test_cases'].text.to_i
  fails = stats.elements['failed_test_cases'].text.to_i
  dirname = nil
  begin
    dir = doc.elements['crpcut/remaining_files'].attributes['nonempty_dir']
    Dir::rmdir(dir)
  rescue SystemCallError
    report = true
    puts "\nworking dir not empty"
  rescue
  end
  t = collection.delete('FAILED')
  count = (t && t.size) || 0
  if count != fail_count then
    puts "\n#{count} failed cases, expected #{fail_count}"
    report = true
  end
  if fails != fail_count then
    puts "\n#{fails} failed in statistics, expected #{fail_count}"
    report = true
  end
  if runs != run_count then
    puts "\n#{runs} run tests in statistics, expected #{run_count}"
    report = true
  end
  if regs != tests.size then
    puts "\n#{regs} registered tests in statistics, expected #{tests.size}"
report = true
  end
  t = collection.delete('OK');
  count = (t && t.size) || 0
  if count != success_count then
    puts "\n#{count} successful found, expected #{success_count}"
    report = true
  end
  if collection.size > 0 then
    puts "\n"
    report = true
    collection.each do |r, l|
      puts r
      l.each { |name| puts "  #{name}" }
    end
  end
  puts "OK" if !report
  !report
end

RUNS={
  "            default_success" =>
  [ /default_success/,    /OK/,      1,  0,  0, BLOCKED_TESTS ],

  "            asserts" =>
  [ /^asserts::/,         /FAILED/, 25, 14,  0, BLOCKED_TESTS ],

  " -v         asserts" =>
  [ /^asserts::/,         /.*/,     25, 14, 11, BLOCKED_TESTS ],

  " -c 8       asserts" =>
  [ /^asserts::/,         /FAILED/, 25, 14,  0, BLOCKED_TESTS ],

  " -c 8 -v    asserts" =>
  [ /^asserts::/,         /.*/,     25, 14, 11, BLOCKED_TESTS ],

  " -n         asserts" =>
  [ /^asserts::/,         /FAILED/, 25, 14,  0, [] ],

  " -n -v      asserts" =>
  [ /^asserts::/,         /.*/,     25, 14, 11, [] ],

  " -n -c 8    asserts" =>
  [ /^asserts::/,         /FAILED/, 25, 14,  0, [] ],

  " -n -c 8 -v asserts" =>
  [ /^asserts::/,         /.*/,     25, 14, 11, [] ],

  "            asserts death" =>
  [ /^(asserts|death)::/, /FAILED/, 41, 26,  0, BLOCKED_TESTS ],

  " -v         asserts death" =>
  [ /^(asserts|death)::/, /.*/,     41, 26, 15, BLOCKED_TESTS ],

  " -c 8       asserts death" =>
  [ /^(asserts|death)::/, /FAILED/, 41, 26,  0, BLOCKED_TESTS ],

  " -c 8 -v    asserts death" =>
  [ /^(asserts|death)::/, /.*/,     41, 26, 15, BLOCKED_TESTS ],

  " -n         asserts death" =>
  [ /^(asserts|death)::/, /FAILED/, 41, 26,  0, [] ],

  " -n -v      asserts death" =>
  [ /^(asserts|death)::/, /.*/,     41, 26, 15, [] ],

  " -n -c 8    asserts death" =>
  [ /^(asserts|death)::/, /FAILED/, 41, 26,  0, [] ],

  " -n -c 8 -v asserts death" =>
  [ /^(asserts|death)::/, /.*/,     41, 26, 15, [] ],

  ""         =>
  [ /.*/,                 /FAILED/, tests.size - BLOCKED_TESTS.size, fails,  0, BLOCKED_TESTS ],

  " -v"      =>
  [ /.*/,                 /.*/,     tests.size - BLOCKED_TESTS.size, fails, oks - BLOCKED_TESTS.size, BLOCKED_TESTS ],

  " -c 8"    =>
  [ /.*/,                 /FAILED/, tests.size - BLOCKED_TESTS.size, fails,  0, BLOCKED_TESTS ],

  " -c 8 -v" =>
  [ /.*/,                 /.*/,     tests.size - BLOCKED_TESTS.size, fails, oks - BLOCKED_TESTS.size, BLOCKED_TESTS ],

  " -n"         =>
  [ /.*/,                 /FAILED/, tests.size, fails,  0,                       [] ],

  " -n -v"      =>
  [ /.*/,                 /.*/,     tests.size, fails, oks, [] ],

  " -n -c 8"    =>
  [ /.*/,                 /FAILED/, tests.size, fails,  0,                       [] ],

  " -n -c 8 -v" =>
  [ /.*/,                 /.*/,     tests.size, fails, oks, [] ],

  " -x -c 8 -o /tmp/crpcutst$$ -q;v=$?;cat /tmp/crpcutst$$;rm /tmp/crpcutst$$;exit $v" =>
  [ /.*/,                 /FAILED/, tests.size - BLOCKED_TESTS.size, fails,  0, BLOCKED_TESTS ]
}

File.open("apafil", 'w') { |f| f.write("apa\n") }
ulimit = open("|bash -c \"ulimit -c\"").read.to_i
if ulimit == 0 then
  puts "You must allow core dumps for the selt test to succeed."
  puts "Do that by issuing the command:"
  puts "> ulimit -c 100000"
  exit 1
end
puts "Self test takes approximately 30 seconds to complete"
RUNS.each do | params, expects |
  print "%-70s: " % "./test/testprog -x -p apa=katt#{params}"
  STDOUT.flush
  exit 1 if !check_file("./test/testprog -x -p apa=katt#{params}", tests, *expects)
end
dirname = "/tmp/crpcut_selftest_dir_#{$$}"
Dir.mkdir(dirname)
prog="./test/testprog -o /dev/null -q -d #{dirname} should_fail_due_to_left_behind_files"
print "%-70s: " % prog
file = open("|#{prog}")
s = file.read
file.close
is_error=false
if !s.empty? then
  puts
  puts "Unexpected stdout for -q"
  is_error = true
end
file_found = nil
begin
file_found = File.stat("#{dirname}/should_fail_due_to_left_behind_files/apa").file?
rescue
end
if !file_found then
  puts if !is_error
  puts "File was not created"
  is_error = true
end
begin
  File.unlink "#{dirname}/should_fail_due_to_left_behind_files/apa"
  Dir.rmdir "#{dirname}/should_fail_due_to_left_behind_files"
  Dir.rmdir dirname
rescue
  puts if !is_error
  puts "Couldn't remove created files"
  is_error = true
end
puts "OK" if !is_error
#File.unlink "./apafil"
