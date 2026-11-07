/* arexxtest.rexx: wxARexxPort against tests/gui/arexxtest (ARXTEST.1).
 * Start arexxtest first (Run), with RexxMast running. Prints PASS or FAIL
 * for each check, then QUITs it. Copyright (c) 2026 AmiCradle Team */
OPTIONS RESULTS
bad = 0
ADDRESS 'ARXTEST.1'

'HELLO world'
CALL check 'HELLO', rc = 0 & result = 'hello world'
'hello there COUNT 3 LOUD'
CALL check 'HELLO args', rc = 0 & result = 'HELLO THERE X3'
'HELLO'
CALL check 'HELLO no NAME', rc = 10
'FAIL'
CALL check 'FAIL', rc = 10
'NOSUCH'
CALL check 'unknown', rc >= 10   /* OS 3.2's arexx.class: 20, ours: 10 */
'WAITCALL'
CALL check 'WAITCALL', rc = 0 & result = 'done'
'WAITNEVER 300'
CALL check 'WAITNEVER', rc = 10
'VERSION'
CALL check 'VERSION', rc = 0 & result = 'arexxtest 1.0'
'HELP'
CALL check 'HELP', rc = 0 & POS('HELLO', result) > 0 & POS('QUIT', result) > 0 & POS('FAULT', result) > 0
'HELP HELLO'
CALL check 'HELP HELLO', rc = 0 & result = 'HELLO NAME/A,COUNT/N,LOUD/S'
'FAULT 205'
CALL check 'FAULT', rc = 0 & LENGTH(result) > 0
'ACTIVATE'
CALL check 'ACTIVATE', rc = 0
'QUIT'
CALL check 'QUIT', rc = 0
IF bad = 0 THEN SAY 'arexxtest: ALL PASS'
ELSE SAY 'arexxtest:' bad 'FAILED'
EXIT bad

check: PARSE ARG what, ok
IF ok THEN SAY 'PASS' what
ELSE DO
  SAY 'FAIL' what '(rc' rc')'
  bad = bad + 1
END
RETURN
