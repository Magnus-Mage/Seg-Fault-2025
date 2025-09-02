\ Enhanced FORTH test program for Phase 2 testing
\ This demonstrates all lexical features

\ Simple word definitions
: HELLO ." Hello, ESP32 World!" ;
: SQUARE DUP * ;
: CUBE DUP SQUARE * ;

\ Mathematical operations
: DISTANCE 
    SWAP DUP *     \ x^2
    SWAP DUP *     \ y^2  
    + SQRT         \ sqrt(x^2 + y^2)
;

\ Complex math with trigonometry
: HYPOTENUSE-ANGLE
    SWAP ATAN      \ angle = atan(opposite/adjacent)
    180 * 3.14159 /  \ convert to degrees
;

\ Control flow structures
: ABS-VALUE
    DUP 0 <
    IF NEGATE THEN
;

: FACTORIAL
    DUP 1 <=
    IF DROP 1
    ELSE
        DUP 1 - FACTORIAL *
    THEN
;

\ Loop structures
: COUNT-DOWN
    BEGIN
        DUP .
        1 -
        DUP 0 <=
    UNTIL
    DROP
;

\ Variable usage (will be implemented in parser phase)
VARIABLE COUNTER
CONSTANT PI 3.14159

\ Test expressions
42 SQUARE .
5 3 DISTANCE .
10 FACTORIAL .
5 COUNT-DOWN

\ Math operations showcase
45 SIN .          \ trigonometry
2.718 LOG .       \ logarithm  
16 SQRT .         \ square root
2 10 POWER .      \ exponential

\ Bitwise operations
255 170 AND .     \ bitwise and
15 240 OR .       \ bitwise or
255 NOT .         \ bitwise not

\ String output
." Testing complete!" 

\ More complex nested definition
: COMPLEX-CALC
    ( n -- result )
    DUP SQUARE      \ n^2
    SWAP CUBE       \ n^3  
    + 2 /           \ (n^2 + n^3) / 2
    ABS-VALUE       \ absolute value
;
