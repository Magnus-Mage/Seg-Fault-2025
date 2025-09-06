\ Test: COMPLEX-CALC structure (no real math yet)
: SQUARE DUP * ;
: CUBE DUP SQUARE * ;
: ABS-VALUE DUP 0 < IF NEGATE THEN ;

: COMPLEX-CALC
    ( n -- result )
    DUP SQUARE
    SWAP CUBE
    + 2 /
    ABS-VALUE
;

3 COMPLEX-CALC .

