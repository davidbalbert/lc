(def caar (x) (car (car x)))
(def cadar (x) (car (cdr (car x))))

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
