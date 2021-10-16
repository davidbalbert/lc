(def num 5)

(def message (quote (hello world)))

(def caaaaar (lambda (x)
  (car (car (car (car (car x)))))))


(caaaaar num)
