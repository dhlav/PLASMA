' CONIOAPI ?ENDSRC
LOOKUP CONIO CONSTANT CONIOAPI
CONIOAPI  3 IFACE PLASMA _HOME     : HOME _HOME DROP ;
CONIOAPI  4 IFACE PLASMA _GOTOXY   : GOTOXY _GOTOXY DROP ;
CONIOAPI  5 IFACE PLASMA _VIEWPORT : VIEWPORT _VIEWPORT DROP ;
CONIOAPI  6 IFACE PLASMA _TEXTTYPE
: NORMALTEXT  $FF _TEXTTYPE DROP ;
: INVERSETEXT $3F _TEXTTYPE DROP ;
: FLASHTEXT   $7F _TEXTTYPE DROP ;
CONIOAPI  7 IFACE PLASMA _TEXT     : TEXT 40 _TEXT DROP ;
CONIOAPI  8 IFACE PLASMA _GR       : GR 1 _GR DROP ;
CONIOAPI  9 IFACE PLASMA _COLOR    : COLOR _COLOR DROP ;
CONIOAPI 10 IFACE PLASMA _PLOT     : PLOT _PLOT DROP ;
CONIOAPI 11 IFACE PLASMA _TONE     : TONE _TONE DROP ;
CONIOAPI 12 IFACE PLASMA RAND
