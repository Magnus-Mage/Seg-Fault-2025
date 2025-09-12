: fib ( n -- n )
  dup 2 < if
    drop 1
  else
    dup 1- recurse
    swap 2 - recurse
    +
  then
;

10 fib .

