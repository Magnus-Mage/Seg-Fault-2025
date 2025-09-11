: test-if ( n -- )
  dup 10 > if
    ." Greater than 10" cr
  else
    ." Less or equal to 10" cr
  then
;

15 test-if
8 test-if

