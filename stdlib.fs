: dup ( x -- x x ) sp@ @ ;

: -1 ( x -- x -1 ) dup dup nand dup dup nand nand ;
: 0 -1 dup nand ;
: 1 -1 dup + dup nand ;

: invert ( x -- !x ) dup nand ;
: and ( x y -- x&y ) nand invert ;
: negate ( x -- -x ) invert 1 + ;
: - ( x y -- x-y ) negate + ;

( numbers )
: 2 1 1 + ;
: 3 2 1 + ;
: 4 2 2 + ;
: 5 3 2 + ;
: 6 3 3 + ;
: 8 4 4 + ;
: 16 8 8 + ;
: 2* ( x -- 2x ) dup + ;

: = ( x y -- flag ) - 0= ;
: <> ( x y -- flag ) = invert ;

( stack )
: drop ( x y -- x ) dup - + ;
: over ( x y -- x y x ) sp@ 1 - @ ;
: swap ( x y -- y x ) over over sp@ 3 - ! sp@ 1 - ! ;
: nip ( x y -- y ) swap drop ;
: 2dup ( x y -- x y x y ) over over ;
: 2drop ( x y -- ) drop drop ;

: or ( x y -- x|y ) invert swap invert and invert ;

: , ( x -- ) here @ ! here @ 1 + here ! ;

\ make words immediate
: immediate latest @ 1 + dup @ 16 or swap ! ;

\ control interpreter state
: [ 0 state ! ; immediate
: ] 1 state ! ;

\ unconditional branch
: branch ( r:addr -- r:addr+offset ) rp@ @ dup @ + rp@ ! ;

\ conditional branch when top of stack is 0
: ?branch ( r:addr -- r:addr | r:addr+offset)
    0= rp@ @ @ 1 - and rp@ @ + 1 + rp@ ! ;

\ lit pushes the value on the next cell to the stack at runtime
\ e.g. lit [ 42 , ] pushes 42 to the stack
: lit ( -- x ) rp@ @ dup 1 + rp@ ! @ ;

\ ['] is identical to lit, the choice of either depends on context
\ don't write as : ['] lit ; as that will break lit's assumptions about
\ the return stack
: ['] ( -- addr ) rp@ @ dup 1 + rp@ ! @ ;

\ push/pop return stack
: >rexit ( addr r:addr0 -- r:addr )
    rp@ ! ;                 \ override return address with original return
                            \ address from >r
: >r ( x -- r:x)
    rp@ @                   \ get current return address
    swap rp@ !              \ replace top of return stack with value
    >rexit ;                \ push new address to return stack
: r> ( r:x -- x )
    rp@ 1 - @               \ get value stored in return stack with >r
    rp@ @ rp@ 1 - !         \ replace value with address to return from r>
    lit [ here @ 3 + , ]    \ get address to this word's exit call
    rp@ ! ;                 \ make code return to this word's exit call,
                            \ effectively calling exit twice to pop return
                            \ stack entry created by >r
\ rotate stack
: rot ( x y z -- y z x ) >r swap r> swap ;
: -rot ( x y z -- z x y ) rot rot ;
: xor ( x y -- x^y) 2dup and invert -rot or and ;
: 0<> 0= invert ;

\ if/then/else
: if
    ['] ?branch ,       \ compile ?branch to skip if's body when false
    here @              \ get address where offset will be written
    0 ,                 \ compile dummy offset
    ; immediate
: then
    dup                 \ duplicate offset address
    here @ swap -       \ calculate offset from if/else
    swap !              \ store calculated offset for ?branch/branch
    ; immediate
: else
    ['] branch ,        \ compile branch to skip else's body when true
    here @              \ get address where offset will be written
    0 ,                 \ compile dummy offset
    swap                \ bring if's ?branch offset address to top of stack
    dup here @ swap -   \ calculate offset from if
    swap !              \ store calculated offset for ?branch
    ; immediate

\ begin...while...repeat and begin...until loops
: begin
    here @              \ get location to branch back to
    ; immediate
: while
    ['] ?branch ,       \ compile ?branch to terminate loop when false
    here @              \ get address where offset will be written
    0 ,                 \ compile dummy offset
    ; immediate
: repeat
    swap                        \ offset will be negative
    ['] branch , here @ - ,     \ compile branch back to begin
    dup here @ swap - swap !    \ compile offset from while
    ; immediate
: until
    ['] ?branch , here @ - ,    \ compile ?branch back to begin
    ; immediate

\ do...loop loops
: do ( end index -- )
    here @                      \ get location to branch back to
    ['] >r , ['] >r ,           \ at runtime, push inputs to return stack
    ; immediate
: loop
    ['] r> , ['] r> ,           \ move current index and end to data stack
    ['] lit , 1 , ['] + ,       \ increment index
    ['] 2dup , ['] = ,          \ index equals end?
    ['] ?branch , here @ - ,    \ when false, branch back to do
    ['] 2drop ,                 \ discard index and end when loop terminates
    ; immediate

: bl ( -- spc ) lit [ 1 2* 2* 2* 2* 2* , ] ;
: cr lit [ 4 6 3 + + , ] lit [ 4 6 + , ] emit emit ;
: space bl emit ;

\ print string
: type ( addr len -- ) 0 do dup @ emit 1 + loop drop ;

\ read char from terminal input buffer, advance >in
: in> ( "c<input>" -- c ) >in @ @ >in dup @ 1 + swap ! ;

\ parse input with specified delimiter
: parse ( delim "input<delim>" -- addr len )
    in> drop                    \ skip space after parse
    >in @                       \ put address of parsed input on stack
    swap 0 begin                \ ( addr delim len )
        over in>                \ ( addr delim len delim char )
    <> while
        1 +                     \ ( addr delim len+1 )
    repeat swap                 \ ( addr len delim )
    bl = if
        >in dup @ 1 - swap !    \ move >in back 1 char if delimiter is bl,
                                \ otherwise the interpreter is left in a
                                \ bad state
    then ;

\ parse input with specified delimiter, skipping leading delimiters
: word ( delim "<delims>input<delim>" -- addr len )
    in> drop                    \ skip space after word
    begin dup in> <> until      \ skip leading delimiters
    >in @ 2 - >in !             \ "put back" last char read from tib,
                                \ and backtrack >in leading char that will
                                \ be skipped by parse
    parse ;

\ read literal string from word body
: litstring ( -- addr len )
    rp@ @ dup 1 + rp@ ! @   \ push length to stack
    rp@ @                   \ push string address to stack
    swap
    2dup + rp@ ! ;          \ move return address past string

\ parse word, compile first char as literal
: [char] ( "<spcs>input<spc>" -- c )
    ['] lit , bl word drop @ , ; immediate

: ." ( "input<quote>" -- )
    [char] " parse                      \ parse input up to "
    state @ if
        ['] litstring ,                 \ compile litstring
        dup ,                           \ compile length
        0 do dup @ , 1 + loop drop      \ compile string
        ['] type ,                      \ display string at runtime
    else
        type                            \ display string
    then ; immediate

\ reserve bytes in dictionary
: allot ( x -- ) here @ + here ! ;

: 10 lit [ 4 4 2 + + , ] ;
: 10h lit [ 4 4 4 4 + + + , ] ;
: 80h lit [ 1 2* 2* 2* 2* 2* 2* 2* , ] ;
: 8000h lit [ 80h 2* 2* 2* 2* 2* 2* 2* 2* , ] ;
: >= ( x y -- flag ) - 8000h and 0= ;
: < ( x y -- flag ) >= invert ;
: <= ( x y -- flag ) 2dup < -rot = or ;
: > ( x y -- flag ) <= invert ;
: 0< ( x -- flag ) 0 < ;

\ creates a word that pushes the address to its body at runtime
: create : ['] lit ,  here @ 2 + , ['] exit , 0 state ! ;

\ creates a single-cell variable initialised with zero
: variable create 0 , ;

\ divison and modulo
: /mod ( x y -- x%y x/y )
    over 0< -rot                \ remainder negative if dividend is negative
    2dup xor 0< -rot            \ quotient negative if operand signs differ
    dup 0< if negate then       \ make divisor positive if negative
    swap dup 0< if negate then  \ make dividend positive if negative
    0 >r begin                  \ hold quotient in return stack
        over 2dup >=            \ while divisor greater than dividend
    while
        -                       \ subtract divisor from dividend
        r> 1 + >r               \ increment quotient
    repeat
    drop nip                    \ leave sign flags and remainder on stack
    rot if negate then          \ set remainder sign
    r> rot                      \ get quotient from return stack
    if negate then ;            \ set quotient sign
: / /mod nip ;
: mod /mod drop ;

variable base
10 base !

\ switch to common bases
: hex 10h base ! ;
: decimal 10 base ! ;
: digit ( x -- c ) dup 10 < if [char] 0 + else 10 - [char] A + then ;

: ?dup dup ?branch [ 2 , ] dup ;

\ print number at the top of the stack in current base
: . ( x -- )
    -1 swap                                     \ put sentinel on stack
    dup 0< if negate -1 else 0 then             \ make positive if negative
    >r                                          \ save sign on return stack
    begin base @ /mod ?dup 0= until             \ convert to base 10 digits
    r> if [char] - emit then                    \ print sign
    begin digit emit dup -1 = until drop        \ print digits
    space ;                                     \ print space

\ base of data stack
: sp0 lit [ sp@ , ] ;

\ print backspace
: backspace lit [ 4 4 + , ] emit ;

\ print stack
: .s
    sp@ 0 swap begin
        dup sp0 >
    while
        1 -
        swap 1 + swap
    repeat swap
    [char] < emit dup . backspace [char] > emit space
    ?dup if
        0 do 1 + dup @ . loop
    then drop ;

1 2 3 80h 10 + .s cr hex .s cr decimal
rot over nip .s cr
." hello world" cr
