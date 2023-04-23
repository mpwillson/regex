# Janet C wrappers for POSIX regex functions

cfun_compile - return compiled extended regex pattern or nil
    `(regex/compile "RE-string")`

cfun_match   - return Janet array of strings matching pattern or nil
    `(regex/match RE text)`

cfun_replace - returns Janet buffer, replacing matched string(s) or nil
    `(regex/replace RE text rep &opt :all)`

To install and test locally:

``` shell
jpm install --local
jpm -l test
```
## Example usage

``` lisp
$ janet
Janet 1.27.0-meson freebsd/x64/clang - '(doc)' for help
repl:1:> (import regex)
@{_ @{:value <cycle 0>} regex/compile @{:private true} regex/match @{:private true} regex/replace @{:private true} :macro-lints @[]}
repl:2:> (def p (regex/compile "a(.)c"))
<regex/pattern 8232e5500>
repl:3:> (regex/match p "abc")
@["abc" "b"]
repl:4:> (regex/replace p "abc azc" "%1")
@"b azc"
repl:5:> (regex/replace p "abc azc" "%1" :all)
@"b z"
```
