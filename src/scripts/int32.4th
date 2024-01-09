' DVAR ENDSRC
" INT32"   LOADMOD"  "
LOOKUP ZERO32     PLASMA ZERO32 
LOOKUP ZEXT16TO32 PLASMA ZEXT32
LOOKUP NEG32      PLASMA NEG32
LOOKUP LOAD32     PLASMA LOAD32
LOOKUP LOADI16    PLASMA LOAD16
LOOKUP STORE32    PLASMA STORE32
LOOKUP ADD32      PLASMA ADD32
LOOKUP ADDI16     PLASMA ADD16
LOOKUP SUB32      PLASMA SUB32
LOOKUP SUBI16     PLASMA SUB16
LOOKUP SHL32      PLASMA SHL32
LOOKUP SHR32      PLASMA SHR32
LOOKUP MUL32      PLASMA MUL32
LOOKUP MULI16     PLASMA MUL16
LOOKUP DIV32      PLASMA DIV32
LOOKUP DIVI16     PLASMA DIV16
LOOKUP ISEQ32     PLASMA ISEQ32
LOOKUP ISEQI16    PLASMA ISEQ16
LOOKUP IDGE32     PLASMA ISGE32
LOOKUP ISGEI16    PLASMA ISGE16
LOOKUP ISLE32     PLASMA ISLE32
LOOKUP ISLEI16    PLASMA ISLE16
LOOKUP ISGT32     PLASMA ISGT32
LOOKUP ISGTI16    PLASMA ISGT16
LOOKUP ISLT32     PLASMA ISLT32
LOOKUP ISLTI16    PLASMA ISLT16
LOOKUP I32TOS     PLASMA I32TOS
LOOKUP PUTI32     PLASMA PUTI32
: DVAR CREATE 4 ALLOT ;
DVAR _DOP1
DVAR _DOP2
: D@ DUP @ SWAP 2+ @ ;
: D! DUP ROT SWAP 2+ ! ! ;
: D+ _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 ADD32 _DOP1 STORE32 _DOP1 D@ ;
: D- _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 SUB32 _DOP1 STORE32 _DOP1 D@ ;
: D* _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 MUL32 _DOP1 STORE32 _DOP1 D@ ;
: D/ _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 DIV32 _DOP1 STORE32 _DOP1 D@ ;
: D< _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 ISLT32 ;
: D> _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 ISGT32 ;
: D= _DOP2 D! _DOP1 D! _DOP1 LOAD32 _DOP2 ISEQ32 ;
: D0= OR 0= ;
: D. _DOP1 D! _DOP1 PUTI32 SPACE ;