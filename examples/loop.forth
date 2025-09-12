: countdown ( n -- )
  begin
    dup 0 >
  while
    dup . 1 -
  repeat
  drop
;

5 countdown

