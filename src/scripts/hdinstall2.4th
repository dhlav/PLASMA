SRC" plasma.4th"
SRC" conio.4th"

: [THEN] ; IMMEDIATE ( PLACE HOLDER TO RESUME EXECUTION )
: [IF] ( F -- )
  NOT IF ( SKIP CODE IN BETWEEN [ELSE] OR [THEN] )
    1 >R
    BEGIN
      BL WORD FIND IF
        CASE
          ' [THEN] OF
            R> 1- ?DUP 0= IF ( EXIT AT FINAL [THEN] )
              DROP EXIT
            THEN
            >R
            ENDOF
          [ LATEST ] LITERAL OF ( CHECK FOR NESTED [IF] )
            R> 1+ >R
            ENDOF
        ENDCASE
      ELSE
        DROP
      THEN
    AGAIN
  THEN
; IMMEDIATE
: STRING CREATE 256 ALLOT DOES> ; ( JUST ALLOCATE THE BIGGEST STRING POSSIBLE )

: CONFIRM" ( -- F )
  COMPILE ."
  ."  (Y/N)"
  KEY CR TOUPPER CHAR Y =
;

: INSERT.FLOPPY"
  ." Insert "
  COMPILE ."
  ."  into floppy drive" CR
  ."  Press any key to continue" KEY DROP CR
;

STRING DEST
STRING FILELIST

0 0 40 24 VIEWPORT
HOME
8 SPACES INVERSETEXT
." PLASMA HD INSTALL STAGE 2"
NORMALTEXT CR CR
0 1 40 23 VIEWPORT

DEST GETPFX DROP

INSERT.FLOPPY" PLASMA.SYS"
FILELIST " -R /PLASMA.SYS/* " STRCPY DEST STRCAT
" COPY" SWAP LOADMOD

CONFIRM" Copy demos?" [IF]
  INSERT.FLOPPY" PLASMA.DEMOS"
  FILELIST " -R /PLASMA.DEMOS/DEMOS " STRCPY DEST STRCAT
  " COPY" SWAP LOADMOD
[THEN]

CONFIRM" Copy floating point libraries?" [IF]
  INSERT.FLOPPY" PLASMA.FPSOS"
  FILELIST " -R /PLASMA.FPSOS/SYS " STRCPY DEST STRCAT
  " COPY" SWAP LOADMOD
[THEN]

CONFIRM" Copy networking libraries?" [IF]
  INSERT.FLOPPY" PLASMA.INET"
  FILELIST " -R /PLASMA.INET/* " STRCPY DEST STRCAT
  " COPY" SWAP LOADMOD
[THEN]

CONFIRM" Copy build tools?" [IF]
  DEST " BLD" STRCAT
  " NEWDIR" SWAP LOADMOD
  INSERT.FLOPPY" PLASMA.BLD"
  FILELIST " -R /PLASMA.BLD/BLD/PLASM /PLASMA.BLD/BLD/CODEOPT " STRCPY
  " /PLASMA.BLD/BLD/INC " STRCAT DEST STRCAT
  " COPY" SWAP LOADMOD

  CONFIRM" Copy sample PLASMA code?" [IF]
    FILELIST " -R /PLASMA.BLD/BLD/SAMPLES " STRCPY DEST STRCAT
    " COPY" SWAP LOADMOD
  [THEN]

  CONFIRM" Copy sample FORTH scripts?" [IF]
    FILELIST " -R /PLASMA.BLD/BLD/SCRIPTS " STRCPY DEST STRCAT
    " COPY" SWAP LOADMOD
  [THEN]

[THEN]

FILELIST " AUTORUN HDINSTALL.4TH" STRCPY
" DEL" SWAP LOADMOD

0 0 40 24 VIEWPORT
HOME
." Done" CR
BYE
