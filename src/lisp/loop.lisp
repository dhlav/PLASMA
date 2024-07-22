(DEFINE
  (TAILLOOP (LAMBDA (I M)
    (COND ((AND (< I M) (PRI I)),(TAILLOOP (+ 1 I) M))
          (T,(EQ I M)))
  ))
  (WHILELOOP (LAMBDA (I M)
    (WHILE (< I M) (PRI I) (SETQ I (+ I 1)))
  ))
  (UNTILLOOP (LAMBDA (I M)
    (UNTIL (> I M) (PRI I) (SETQ I (+ I 1)))
  ))
  (PROGLOOP (LAMBDA (I M)
    (PROG (X)
      (SETQ X (- M 1))
    A (PRI I)
      (SETQ I (+ I 1))
      (IF (< I X) (GO A))
      (RETURN I)
  )))
  (FORLOOP (LAMBDA (I M)
    (FOR I 1 1 (< I M) (PRI I))
  ))
)

'TAIL
(TAILLOOP 1 100)
'WHILE
(WHILELOOP 1 100)
'UNTIL
(UNTILLOOP 1 100)
'PROG
(PROGLOOP 1 100)
'FOR
(FORLOOP 1 100)
