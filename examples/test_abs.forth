\ Test: ABS-VALUE with IF/THEN control flow
: ABS-VALUE
    DUP 0 <
    IF NEGATE THEN
;

-10 ABS-VALUE .
0 ABS-VALUE .
10 ABS-VALUE .

