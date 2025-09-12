: factorial ( n -- n! )
  dup 1 > if
    dup 1- factorial *
  else
    drop 1
  then
;

5 factorial .

