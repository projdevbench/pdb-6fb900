(not #f)	#t
(not (void))	#f
(pair? (car (cons 1 2)))	#f
(symbol? (quote var))	#t
(number? (+ 5 1))	#t
(null? (quote ()))	#t
(eq? 3 (+ 1 2))	#t
(eq? #t (= 0 0))	#t
(eq? (quote ()) (quote ()))	#t
(eq? (quote (1 2 3)) (quote (1 2 3)))	#f
(define str "hello")	
(eq? str str)	#t
(eq? "hello" "hello")	#f
(eq? 1 1)	#t
(- 10 3)	7
(* 2 3)	6
(/ 29 3)	29/3
(/ 9 6)	3/2
(/ 1 0)	RuntimeError
(+ 1/2 1)	3/2
(* (+ 2 3) (- 5 3))	10
(/ (+ 9 1) (+ 2 3))	2
(+ (/ 1 2) (+ 1 1))	5/2
(+)	0
(*)	1
(-)	RuntimeError
(/)	RuntimeError
(- 5)	-5
(/ 4)	1/4
(+ 2 3 4)	9
(- 2 3 4)	-5
(* 2 3 4)	24
(/ 2 3 4)	1/6
(= 2 2 2)	#t
(< 2 3 4)	#t
(> 4 1 -1)	#t
(<= 1 1 1)	#t
(>= 2 1 1)	#t
(< 1)	RuntimeError
(<)	RuntimeError
(< 2 1 (quote a))	#f
(number? (/ 4 2))	#t
(number? (+ 1/2 1/2))	#f
(< 1 3 2)	#f
(< 1/2 2/3)	#t
(cons (quote a) (cons 3 "hello"))	(a 3 . "hello")
(cons (cons 0 1) (cons 2 3))	((0 . 1) 2 . 3)
(car (quote (1 2 3 4)))	1
(cdr (quote (1 2 3 4)))	(2 3 4)
(car (cons 5 #f))	5
(cdr (cons #t (void)))	#<void>
(cdr (quote ((ll . lr) . (rl . rr))))	(rl . rr)
(car (quote (+ - *)))	+
(list)	()
(list 1)	(1)
(list (quote (1 2)) (quote (3 4)))	((1 2) (3 4))
(list? (quote ()))	#t
(list? (quote (1 2 3)))	#t
(pair? (quote ()))	#f
(list? (cons 1 2))	#f
(list? (cons 1 (cons 2 (quote ()))))	#t
(quote 1)	1
(quote #t)	#t
(quote (+ 1 2 3))	(+ 1 2 3)
(quote (4 . 5))	(4 . 5)
(quote scheme)	scheme
(quote (1 2 . 3))	(1 2 . 3)
(quote (1 . (2 . 3)))	(1 2 . 3)
(quote (1 . (2 . (3 . ()))))	(1 2 3)
(quote (quote (1 2)))	(quote (1 2))
(car (quote ((1 . 2) 3 . 4)))	(1 . 2)
(cdr (quote ((1 . 2 ) 3 . 4)))	(3 . 4)
(quote (quote () 1 2 . (4 . 2)))	(quote () 1 2 4 . 2)
(car (quote (quote () 1 2 . (4 . 2))))	quote
(cdr (quote (quote () 1 2 . (4 . 2))))	(() 1 2 4 . 2)
'(a . b . c)	RuntimeError
'(a .)	RuntimeError
(not (> 1 2))	#t
(not 5)	#f
(and)	#t
(and 1 2 3)	3
(and 1 2 3 #f)	#f
(or)	#f
(or 1 2 3)	1
(or #f 1 2 3)	1
(or #f #f #f)	#f
(and #f (/ 1 0))	#f
(if 0 1 2)	1
(if (< 2 1) #f #t)	#t
(if (void) undefined 1)	RuntimeError
(if #f undefined 1)	1
(if #t 1 (+ 1))	1
(if "hello" 1 2)	1
(if #t 'true-branch (/ 1 0))	true-branch
(if #f (/ 1 0) 'false-branch)	false-branch
(cond (else 1) (#t 2))	1
(cond (#t))	#t
(cond (#f 1))	#<void>
(cond ((= 1 2) 'a) ((= 1 1) 'b 'c) (else 'd))	c
(begin 1)	1
(begin (void) (cons 1 2) #t)	#t
undefined	RuntimeError
+	#<procedure>
((if #t + -) 1 2)	3
(lambda (x y) (+ x y))	#<procedure>
(lambda () (void))	#<procedure>
(lambda (void) undefined)	#<procedure>
(((lambda (+) +) *) 2 3)	6
((lambda (x y) (* x y)) 10 11)	110
(define vhello "Hello world")	
vhello	"Hello world"
(define fhello (lambda () "Hello world"))	
fhello	#<procedure>
(fhello)	"Hello world"
(define sum3 (lambda (a b c)(+ a b c)))	
(sum3 1 2 3)	6
(define (sum3b a b c) (+ a b c))	
(sum3b 1 2 3)	6
(define (fact n) (if (= n 1) 1 (* n (fact (- n 1)))))	
(fact 10)	3628800
(define (list*2 ls) (if (null? ls) '() (cons (* 2 (car ls)) (list*2 (cdr ls)))))	
(list*2 '(1 2 3))	(2 4 6)
(let ((i 1) (j 2)) (+ i j))	3
(let ((i 1)) (let ((j (+ i 2))) (* i j)))	3
(let ((i 1) (j (+ i 2)))(* i j))	RuntimeError
(let ((x (quote inner))) (let ((func (lambda () x))) (let ((x (quote outer))) (func))))	inner
(let ((+ 1)) +)	1
(let ((+ -)) (+ 2 1))	1
(let ((x (lambda (y) y))) x)	#<procedure>
(let ((fct (lambda (n) (if (= n 0) 1 (* n (fct (- n 1))))))) (fct 5))	RuntimeError
(letrec ((a 1) (b a)) b)	RuntimeError
(letrec ((a 1) (b (lambda () a))) (b))	1
(letrec ((fact (lambda (n) (if (<= n 1) 1 (* n (fact (- n 1))))))) (fact 5))	120
(define var 1)	
(set! var (* var 10))	
var	10
(let ((i 1)) (set! i (+ i 3)) i)	4
(define tree '((1 2) (3 4 5) (6 7 8 9)))	
(set-car! (car tree) 100)	
tree	((100 2) (3 4 5) (6 7 8 9))
(define (is-even n) (cond ((= n 0) #t) (else (is-odd (- n 1)))))	
(define (is-odd n) (cond ((= n 0) #f) (else (is-even (- n 1)))))	
(is-even 100)	#t
(is-odd 7)	#t
(define bank-account (let ((balance 10)) (lambda (n) (set! balance (+ balance n)) balance)))	
(bank-account 20)	30
(bank-account -25)	5
(define (make-bank-account balance) (lambda (n) (set! balance (+ balance n)) balance))	
(define gates-bank-account (make-bank-account 10))	
(gates-bank-account 50)	60
(gates-bank-account -55)	5
(define torvalds-bank-account (make-bank-account 100))	
(torvalds-bank-account -70)	30
(torvalds-bank-account 300)	330
(void)	#<void>
(begin (void))	#<void>
(set! undefinedvar 1)	RuntimeError
(display "hi")	hi
(display '(1 "a" b))	(1 "a" b)
(define (f) (define a 1) (define (g) (+ a b)) (define b 2) (g))	
(f)	3
(define (h x) (define (ev? n) (if (= n 0) #t (od? (- n 1)))) (define (od? n) (if (= n 0) #f (ev? (- n 1)))) (ev? x))	
(h 11)	#f
(define (loop n acc) (if (= n 0) acc (loop (- n 1) (+ acc 1))))	
(loop 1000000 0)	1000000
(define (deep n) (if (= n 0) 0 (+ 1 (deep (- n 1)))))	
(deep 100000)	100000
(let loop ((i 0) (acc '())) (if (= i 3) acc (loop (+ i 1) (cons i acc))))	(2 1 0)
(car '())	RuntimeError
(car 1 2)	RuntimeError
(1 2)	RuntimeError
((lambda (x) x))	RuntimeError
(define + 1)	RuntimeError
(define 1x 2)	RuntimeError
4/2	2
(number? 4/2)	#f
(number? 1/2)	#f
(modulo 7 3)	1
(expt 2 10)	1024
(eq? car car)	#t
(map car '((1)))	RuntimeError
(apply + '(1 2))	RuntimeError
(procedure? car)	#t
(car (list 1 2))	1
((lambda (f) (f '(1 2))) cdr)	(2)
(let ((list (lambda args 1))) 1)	RuntimeError
(define x 5)	
(define x 6)	
x	6
(string? "a")	#t
(boolean? '#f)	#t
(let ((p (cons 1 2))) (set-cdr! p p) (list? p))	#f
"str"	"str"
(define lst (list 1 2))	
(set-cdr! lst 5)	
lst	(1 . 5)
(cond ((assq 1 2) 1))	RuntimeError
(let () 5)	5
(letrec ((even? (lambda (n) (if (= n 0) #t (odd? (- n 1))))) (odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))) (even? 1001))	#f
(if #f #f)	
[+ 1 2]	3
(let ([x 5]) x)	5
(exit)	
