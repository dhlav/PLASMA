;
; LORES GRAPHICS TRIG PLOTTING EXAMPLE
;
(DEFINE
  (PLOTFUNC (LAMBDA (FN)
    (PROG (I X Y)
      (FOR I 0 1 (< I 40)
        (SETQ X (/ (- I 19.5) 20.0))
        (SETQ Y (FN X))
        (AND (> Y -1.0) (< Y 1.0)
          (PLOT I (- 19.5 (* Y 20.0))))
      )
      (RETURN 0)
    )
  ))
  ;
  ; USE FUNCTION TO PASS IN LAMBDA EQUATIONS
  ; BEST OPTION FOR GENERIC CASE
  ;
  (PLOTSIN (LAMBDA ()
    (PLOTFUNC (FUNCTION (LAMBDA (S) (SIN (* S *PI*)))))
  ))
  ;
  ; USE QUOTE TO PASS IN LAMBDA EQUATION
  ; ONLY APPLICABLE IF NO FREE VARIABLES
  ;
  (PLOTCOS (LAMBDA ()
    (PLOTFUNC '(LAMBDA (S) (COS (* S *PI*))))
  ))
)
(GR T)
(COLOR 2)
(PLOTSIN)
(COLOR 9)
(PLOTCOS)
"Press a key..."
(READKEY)
(GR F)
