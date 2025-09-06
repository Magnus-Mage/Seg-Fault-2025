\ FORTH-ESP32 Compiler Test Program
\ Phase 4: Comprehensive Testing of All Features

\ Simple number literals and basic math
42 17 + .
." Result should be 59" CR

\ Basic stack operations
10 DUP * .
." Result should be 100 (10 squared)" CR

\ Word definition - simple
: DOUBLE DUP + ;
5 DOUBLE .
." Result should be 10" CR

\ Word definition - more complex
: SQUARE DUP * ;
: CUBE DUP SQUARE * ;
4 CUBE .
." Result should be 64 (4 cubed)" CR

\ String handling
." Hello, FORTH World!" CR
" This is a regular string"
." String on stack has length: "
DUP .
." and starts at address: "
SWAP . CR

\ Variables and constants
VARIABLE COUNTER
42 COUNTER !
COUNTER @ .
." Counter value should be 42" CR

3.14159 CONSTANT PI
PI .
." PI should be approximately 3.14159" CR

\ Control flow - IF/THEN/ELSE
: ABS-VALUE
    DUP 0 < IF
        NEGATE
    THEN
;

-15 ABS-VALUE .
." Absolute value of -15 should be 15" CR

: SIGN-TEST
    DUP 0 > IF
        ." Positive"
    ELSE
        DUP 0 < IF
            ." Negative"
        ELSE
            ." Zero"
        THEN
    THEN
    DROP
;

10 SIGN-TEST CR
-5 SIGN-TEST CR
0 SIGN-TEST CR

\ Loops - BEGIN/UNTIL
: COUNTDOWN
    BEGIN
        DUP .
        1 -
        DUP 0 <=
    UNTIL
    DROP
;

." Countdown from 5: "
5 COUNTDOWN CR

\ Complex word with multiple features
: FACTORIAL
    DUP 1 <= IF
        DROP 1
    ELSE
        DUP 1 - FACTORIAL *
    THEN
;

." Factorial tests:" CR
1 FACTORIAL . ." (1! = 1)" CR
5 FACTORIAL . ." (5! = 120)" CR
6 FACTORIAL . ." (6! = 720)" CR

\ Advanced math operations
: HYPOTENUSE
    SWAP DUP * SWAP DUP * + SQRT
;

." Hypotenuse of 3,4 triangle: "
3 4 HYPOTENUSE . CR

\ Stack manipulation tests
: STACK-TEST
    1 2 3 4 5           \ Put 5 numbers on stack
    ." Initial: 1 2 3 4 5" CR
    
    ." Top value: " DUP . CR
    ." After DUP: " . . ." (should print 5 5)" CR
    
    10 20 SWAP          \ Test SWAP
    ." After 10 20 SWAP: " . . ." (should print 10 20)" CR
    
    100 200 300 ROT     \ Test ROT
    ." After 100 200 300 ROT: " . . . ." (should print 200 300 100)" CR
;

STACK-TEST

\ Nested control structures
: COMPLEX-LOGIC
    DUP 0 > IF
        DUP 10 < IF
            ." Small positive: " DUP . CR
        ELSE
            DUP 100 < IF
                ." Medium positive: " DUP . CR
            ELSE
                ." Large positive: " DUP . CR
            THEN
        THEN
    ELSE
        DUP 0 < IF
            ." Negative: " DUP . CR
        ELSE
            ." Zero: " DUP . CR
        THEN
    THEN
    DROP
;

." Testing complex nested logic:" CR
5 COMPLEX-LOGIC
50 COMPLEX-LOGIC
500 COMPLEX-LOGIC
-10 COMPLEX-LOGIC
0 COMPLEX-LOGIC

\ Recursive definition
: GCD
    SWAP DUP 0 = IF
        DROP
    ELSE
        SWAP OVER MOD GCD
    THEN
;

." GCD tests:" CR
." GCD(48, 18) = " 48 18 GCD . CR
." GCD(17, 13) = " 17 13 GCD . CR

\ Test mathematical functions
." Math function tests:" CR
2 SIN . ." sin(2)" CR
2 COS . ." cos(2)" CR
16 SQRT . ." sqrt(16) = 4" CR
-7 ABS . ." abs(-7) = 7" CR

\ Comparison operations
." Comparison tests:" CR
5 3 > . ." 5 > 3 should be true (-1)" CR
3 5 < . ." 3 < 5 should be true (-1)" CR
4 4 = . ." 4 = 4 should be true (-1)" CR
7 2 <> . ." 7 <> 2 should be true (-1)" CR

\ Memory operations test
VARIABLE TEST-VAR
VARIABLE ANOTHER-VAR

42 TEST-VAR !
." Stored 42 in TEST-VAR" CR
TEST-VAR @ . ." Retrieved from TEST-VAR" CR

\ Advanced loop with complex condition
: SUM-TO-N
    0 SWAP              \ Initialize sum to 0
    BEGIN
        DUP 0 >         \ While n > 0
    WHILE
        SWAP OVER +     \ Add n to sum
        SWAP 1 -        \ Decrement n
    REPEAT
    DROP                \ Remove n (now 0)
;

." Sum from 1 to 10: " 10 SUM-TO-N . CR
." Sum from 1 to 5: " 5 SUM-TO-N . CR

\ Test word redefinition capability
: TEST-WORD ." First definition" CR ;
TEST-WORD

: TEST-WORD ." Redefined version" CR ;
TEST-WORD

\ Complex data manipulation
: BUBBLE-SORT-STEP
    \ Simple bubble sort step (conceptual)
    ." Bubble sort step (implementation depends on array support)" CR
;

\ Final comprehensive test
: COMPREHENSIVE-TEST
    ." === COMPREHENSIVE FORTH TEST ===" CR
    ." Testing all major language features:" CR
    
    ." 1. Arithmetic: " 6 7 * 8 + . CR
    ." 2. Stack ops: " 1 2 3 ROT . . . CR  
    ." 3. Comparison: " 5 5 = . CR
    ." 4. Control flow: " 1 IF ." IF works" THEN CR
    ." 5. Loops: " 3 BEGIN DUP . 1 - DUP 0 <= UNTIL DROP CR
    ." 6. Variables: " VARIABLE TEMP 99 TEMP ! TEMP @ . CR
    ." 7. Words: " 12 SQUARE . CR
    
    ." All tests completed!" CR
;

\ Run the comprehensive test
COMPREHENSIVE-TEST

\ Final message
." " CR
." ============================================" CR
." FORTH-ESP32 Compiler Test Program Complete" CR
." If you see this message, the compiler is" CR
." successfully processing FORTH code!" CR
." ============================================" CR
