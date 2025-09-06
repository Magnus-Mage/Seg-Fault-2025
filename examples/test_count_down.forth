\ Test: COUNT-DOWN using BEGIN/UNTIL
: COUNT-DOWN
    BEGIN
        DUP .
        1 -
        DUP 0 <=
    UNTIL
    DROP
;

5 COUNT-DOWN

