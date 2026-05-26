# mslang 语法规范

## 1. 词法规则

### 1.1 字符集与编码

源文件为 **UTF-8** 编码，不含 BOM。词法分析按字节读取，标识符与字符串字面量可含非 ASCII Unicode 字符。

### 1.2 空白与注释

```
whitespace   = ' ' | '\t' | '\r' | '\n'
line_comment = '//' { any_char_except_newline }
```

注释不产生任何 token。**仅支持单行注释**（`//`），不支持块注释。

### 1.3 自动分号插入（ASI）

词法分析器在行尾扫描时，若当前行最后一个有效 token 属于以下类型之一，则在行尾自动插入虚拟 `;`：

- 标识符
- 整数/浮点/字符串/bytes/bool/nil 字面量
- 关键字：`return`、`break`、`continue`、`fallthrough`
- 右界符：`)`、`]`、`}`
- 后缀运算符：`++`、`--`

这与 Go 的规则一致，使得 `}` 可安全换行到下一行而不中断语句。

`<-`（接收）与 `await` 为前缀算子，ASI 由其**操作数的末 token** 决定（如 `v := <-ch` 末 token 为标识符 `ch`，`r := await f()` 末 token 为 `)`），无需为它们添加额外触发规则。

### 1.4 关键字（保留字）

```
if       else     for      break    continue  return   func
class    extends  self     import   as        var      nil
true     false    and      or       not       in       is
try      catch    finally  raise    go        chan     select
async    await    make     pass
```

> `len`、`type` 为内置全局函数（非保留字），可作为值传递（如 `map(len, lst)`）。`make` 保留为关键字（用于 `make` 表达式）。

### 1.5 标识符

```
letter     = 'A'..'Z' | 'a'..'z' | '_' | unicode_letter
digit      = '0'..'9'
identifier = letter { letter | digit }
```

标识符区分大小写。

### 1.6 整数字面量

```
int_literal = decimal_lit | hex_lit | oct_lit | bin_lit
decimal_lit = '0' | ('1'..'9') { '_'? digit }
hex_lit     = '0' ('x'|'X') hex_digit { '_'? hex_digit }
oct_lit     = '0' ('o'|'O') oct_digit { '_'? oct_digit }
bin_lit     = '0' ('b'|'B') ('0'|'1') { '_'? ('0'|'1') }
```

值域：int64（-2^63 ~ 2^63-1），溢出时回绕。`_` 仅作视觉分隔。

### 1.7 浮点字面量

```
float_literal = decimal_digits '.' decimal_digits? exponent?
              | decimal_digits exponent
              | '.' decimal_digits exponent?
exponent      = ('e'|'E') ('+'|'-')? decimal_digits
```

存储为 IEEE 754 float64。

### 1.8 字符串字面量

```
string_literal = '"' { char | escape_seq } '"'
escape_seq     = '\n' | '\t' | '\r' | '\\' | '\"'
               | '\x' hex_digit hex_digit
               | '\u' '{' hex_digit{1,6} '}'
```

**字符串只支持双引号 `""` 形式**，不支持反引号原始字符串。字符串底层为 UTF-8 字节序列（不可变）。

### 1.9 bytes 字面量

```
bytes_literal = 'b"' { char | escape_seq } '"'
```

产生可变字节数组对象（非字符串类型）。

### 1.10 运算符与界符

```
+   -   *   /   %   **          算术
&   |   ^   <<  >>  ~           位运算
==  !=  <   <=  >   >=          比较
and or  not                     逻辑
=   :=  +=  -=  *=  /=  %=      赋值
&=  |=  ^=  <<=  >>=            复合赋值
->  <-                          channel 收发
.   ,   ;   :   ...             分隔
(   )   [   ]   {   }           界符
++  --                          自增/自减
```

---

## 2. 文法规范（EBNF）

### 2.1 顶层结构

```ebnf
Program     = { Statement | FuncDecl | ClassDecl | ImportDecl } EOF

ImportDecl  = 'import' DottedName [ 'as' identifier ] ';'
DottedName  = [ '.' { '.' } ] identifier { '.' identifier }
              // 无前缀：绝对路径/内置模块，如 math 或 os.path
              // 单个前缀点：相对当前包，如 .utils
              // 多个前缀点：上级包，如 ..common.util

FuncDecl    = [ 'async' ] 'func' identifier '(' ParamList ')' Block
ClassDecl   = 'class' identifier [ 'extends' identifier ] '{' { MethodDecl } '}'
MethodDecl  = [ 'async' ] 'func' identifier '(' ParamList ')' Block

ParamList   = [ identifier { ',' identifier } [ ',' '...' identifier ] ]
```

### 2.2 语句

```ebnf
Statement   = VarDecl
            | ShortVarDecl
            | AssignStmt
            | ExprStmt
            | IfStmt
            | ForStmt
            | ReturnStmt
            | BreakStmt
            | ContinueStmt
            | TryStmt
            | RaiseStmt
            | GoStmt
            | SelectStmt
            | Block
            | ';'

VarDecl      = 'var' identifier [ '=' Expr ] ';'
ShortVarDecl = identifier ':=' Expr ';'
AssignStmt   = LValue ( '=' | '+=' | '-=' | '*=' | '/=' | '%=' ) Expr ';'
             | LValue ( '++' | '--' ) ';'
ExprStmt     = Expr ';'

IfStmt       = 'if' Expr Block [ 'else' ( IfStmt | Block ) ]
ForStmt      = 'for' [ ForHeader ] Block
ForHeader    = Expr                                    (* while 形式 *)
             | [ ShortVarDecl ] Expr ';' [ AssignStmt ]  (* 三段式 *)
             | identifier [ ',' identifier ] 'in' Expr   (* range 形式 *)
ReturnStmt   = 'return' [ Expr ] ';'
BreakStmt    = 'break' ';'
ContinueStmt = 'continue' ';'

TryStmt      = 'try' Block { CatchClause } [ 'finally' Block ]
CatchClause  = 'catch' '(' identifier [ ':' identifier { ',' identifier } ] ')' Block
RaiseStmt    = 'raise' [ Expr ] ';'

GoStmt       = 'go' CallExpr ';'
SelectStmt   = 'select' '{' { SelectCase } '}'
SelectCase   = ( 'case' ( SendStmt | RecvStmt ) | 'default' ) ':' { Statement }
SendStmt     = Expr '<-' Expr
RecvStmt     = [ identifier ':=' ] '<-' Expr

Block        = '{' { Statement | FuncDecl | ClassDecl } '}'
LValue       = identifier | Expr '.' identifier | Expr '[' Expr ']'
```

### 2.3 表达式（优先级从高到低）

| 优先级 | 运算符/形式 | 结合性 |
|---|---|---|
| 13 | `()` `[]` `.` 函数调用 属性访问 下标 | 左 |
| 12 | 一元 `+` `-` `~` `not` `await` | 右 |
| 11 | `**` | 右 |
| 10 | `*` `/` `%` | 左 |
| 9 | `+` `-` | 左 |
| 8 | `<<` `>>` | 左 |
| 7 | `&` | 左 |
| 6 | `^` | 左 |
| 5 | `\|` | 左 |
| 4 | `==` `!=` `<` `<=` `>` `>=` `in` `is` | 左 |
| 3 | `and` | 左 |
| 2 | `or` | 左 |
| 1 | 三目 `a if cond else b` | 右 |

```ebnf
Expr        = TernaryExpr
TernaryExpr = OrExpr [ 'if' Expr 'else' TernaryExpr ]
OrExpr      = AndExpr { 'or' AndExpr }
AndExpr     = CmpExpr { 'and' CmpExpr }
CmpExpr     = BitOrExpr { ( '=='|'!='|'<'|'<='|'>'|'>='|'in'|'is' ) BitOrExpr }
BitOrExpr   = BitXorExpr { '|' BitXorExpr }
BitXorExpr  = BitAndExpr { '^' BitAndExpr }
BitAndExpr  = ShiftExpr { '&' ShiftExpr }
ShiftExpr   = AddExpr { ( '<<' | '>>' ) AddExpr }
AddExpr     = MulExpr { ( '+' | '-' ) MulExpr }
MulExpr     = PowExpr { ( '*' | '/' | '%' ) PowExpr }
PowExpr     = UnaryExpr [ '**' PowExpr ]
UnaryExpr   = ( '+' | '-' | '~' | 'not' | 'await' | '<-' ) UnaryExpr
            | PostfixExpr
PostfixExpr = PrimaryExpr { ( '.' identifier ) | ( '[' Expr ']' ) | CallArgs }
            | PrimaryExpr ( '++' | '--' )
CallArgs    = '(' [ ArgList ] ')'
ArgList     = Expr { ',' Expr } [ ',' '...' Expr ]

PrimaryExpr = identifier
            | Literal
            | '(' Expr ')'
            | ListLiteral
            | MapLiteral
            | TupleLiteral
            | FuncLiteral
            | MakeExpr
            | RecvExpr

ListLiteral  = '[' [ Expr { ',' Expr } [ ',' ] ] ']'
MapLiteral   = '{' [ MapEntry { ',' MapEntry } [ ',' ] ] '}'
MapEntry     = Expr ':' Expr
TupleLiteral = '(' Expr ',' [ Expr { ',' Expr } ] ')'
FuncLiteral  = [ 'async' ] 'func' '(' ParamList ')' Block
MakeExpr     = 'make' '(' 'chan' [ ',' Expr ] ')'
RecvExpr     = '<-' Expr

Literal      = int_literal | float_literal | string_literal
             | bytes_literal | 'true' | 'false' | 'nil'
```

---

## 3. 语句语义摘要

### 3.1 短变量声明

```ms
x := expr
```

在当前作用域引入新变量。若该名字在同一作用域已存在则为**赋值**（不重复声明，与 Go 多返回值规则一致——至少有一个新变量）。

### 3.2 for 的三种形式

```ms
// while 形式
for condition { ... }

// 三段式
for i := 0; i < 10; i++ { ... }

// range 形式（list / map / string / channel）
for v in list { ... }
for k, v in map { ... }
for ch in channel { ... }   // 直到 channel 关闭
```

### 3.3 函数字面量与闭包

```ms
add := func(x, y) { return x + y }
counter := func() {
    n := 0
    return func() {
        n += 1
        return n
    }
}()
```

闭包捕获外层变量的**引用**（upvalue 机制），生命周期由 GC 管理。

### 3.4 可变参数

```ms
func sum(...args) {
    total := 0
    for v in args { total += v }
    return total
}
sum(1, 2, 3)
```

`...args` 在函数体内为 list。

### 3.5 channel 操作

```ms
ch := make(chan)        // 无缓冲
ch := make(chan, 16)    // 缓冲 16

ch <- value            // 发送（阻塞直到接收方就绪）
value := <-ch          // 接收（阻塞直到有值）

for v in ch { ... }    // 迭代直到 close(ch)
close(ch)
```

### 3.6 async/await

```ms
import io

async func fetchData() {
    return await io.readFile("data.txt")
}

// await 可等待任何 awaitable：async func 的返回值（Future）、channel、内置 awaitable
result := await fetchData()
```

`await` 只能出现在 `async func` 内部。
