# smoke tests
(import regex)

(def p (regex/compile "a.c"))
(def x (regex/compile "xxx$"))
(assert (regex/match p "xxxabcxxx"))
(assert (regex/match x "xxxyyyxxx"))
(assert (deep= (regex/replace x "xxxyyyxxx" "a") @"xxxyyya"))

(def text "Some stuff about projects")

(assert (not (regex/match x text)))
(assert (regex/match (regex/compile "project") text))

(def p (regex/compile "z(.)z"))
(assert (deep= (regex/replace p "zaz" "b%1b") @"bab"))

(def p (regex/compile "^(.*)$"))
(assert (deep= (regex/replace p "aaa" "%1 - %1") @"aaa - aaa"))
(assert (deep= (regex/replace p "aaa" "%%1") @"%1"))

(def p (regex/compile "aa"))
(assert (deep= (regex/replace p @"aabbaaccaa" "zz" :all) @"zzbbzzcczz"))
(assert (deep= (regex/replace p @"aabbaaccaa" "zz") @"zzbbaaccaa"))
(assert (deep= (regex/replace p @"aabbaaccaa" "zz" nil) @"zzbbaaccaa"))

(def p (regex/compile "a[bc]a"))
(assert (deep= (regex/replace p "aaa aba aca aza aca" @"zz" :all)
               @"aaa zz zz aza zz"))
(def p (regex/compile "a([bc])a"))
(assert (deep= (regex/replace p "aaa aba aca aza aca" "%1%1%1" :all)
               @"aaa bbb ccc aza ccc"))

(def p (regex/compile "a(bc[a-m]).*k(.*)ka"))
(assert (deep= (regex/replace p "abcda abcka abcka" @"%1:%2") @"bcd:a abc"))
