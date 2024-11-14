;
; Example from LISP 1.5 manual
;
(define
  (equal (lambda (x y)
    (cond ((and (atom x) (atom y)) (eq x y))
          ((equal (car x) (car y)) (equal (cdr x) (cdr y)))
          (t f)
    ))
  )
  (subst (lambda (x y z)
    (cond ((equal y z) x)
          ((atom z) z)
          (t (cons (subst x y (car z)) (subst x y (cdr z))))
    ))
  )
  (append (lambda (x y)
    (cond ((null x) y)
          (t (cons (car x) (append (cdr x) y)))
    ))
  )
)
