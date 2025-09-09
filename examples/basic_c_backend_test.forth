\ Example FORTH Program for ESP32
\ This demonstrates basic FORTH features that should compile to C

\ Simple math and stack operations
: SQUARE DUP * ;
: CUBE DUP SQUARE * ;

\ Variable and constant examples
VARIABLE COUNTER
42 CONSTANT ANSWER

\ Control flow example
: COUNTDOWN 
    BEGIN 
        DUP .
        1 - 
        DUP 0 = 
    UNTIL 
    DROP 
;

\ Conditional example  
: TEST-VALUE
    DUP 10 < IF
        ." Small number: " .
    ELSE
        ." Large number: " .
    THEN
;

\ String output
: HELLO ." Hello, ESP32 World!" CR ;

\ Main program - this will be called from ESP32
: MAIN
    HELLO
    ." Testing FORTH on ESP32" CR
    
    5 SQUARE . CR
    3 CUBE . CR
    
    10 COUNTDOWN
    
    5 TEST-VALUE
    15 TEST-VALUE
    
    ANSWER . CR
;

\ Execute main program
MAIN
