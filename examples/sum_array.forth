variable sum
0 sum !

: add-to-sum ( n -- )
  sum @ + sum !
;

: sum-array ( addr len -- sum )
  0 sum !
  0 do
    over i + @ add-to-sum
  loop
  sum @
;

create myarray 1 , 2 , 3 , 4 , 5 ,

myarray 5 sum-array .

