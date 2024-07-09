(define
  (member (lambda (a x)
    (cond ((null x) , f)
          ((eq a (car x)) , t)
          (t , (member a (cdr x)))
    ))
  )
  (union (lambda (x y)
    (cond ((null x) , y)
          ((member (car x) y) , (union (cdr x) y))
          (t , (cons (car x) (union (cdr x) y)))
    ))
  )
  (intersection (lambda (x y)
    (cond ((null x) , nil)
          ((member (car x) y) , (cons (car x) (intersection
            (cdr x) y)))
          (t , (intersection (cdr x) y))
    ))
  )
)
