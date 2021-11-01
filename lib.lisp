(def caar (x) (car (car x)))
(def cadr (x) (car (cdr x)))
(def cadar (x) (car (cdr (car x))))

(def list args)

(def map (f l)
    (if (pair? l)
        (cons (f (car l)) (map f (cdr l)))
        nil))

(def let (macro (bindings . body)
    `((fn ,(map car bindings) ,@body) ,@(map cadr bindings))))

(def let* (macro (bindings . body)
    (if (nil? bindings)
        `((fn () ,@body))
        `((fn (,(caar bindings))
            (let* ,(cdr bindings) . ,body)) ,(cadar bindings)))))

(def repeat (v n)
    (if (= n 0)
        nil
        (cons v (repeat v (- n 1)))))

(def letrec (macro (bindings . body)
    `(
        (fn ,(map car bindings)
            ,@(map (fn (b) `(set ,(car b) ,(cadr b))) bindings)
            ,@body)
        ,@(repeat nil (length bindings)))))
