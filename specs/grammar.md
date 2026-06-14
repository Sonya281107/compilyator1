# Грамматика языка Ryst

Грамматика задана в расширенной форме БНФ (EBNF). Терминалы — в кавычках или `snake_case`.

## Лексические правила

```
IDENT    ::= [a-zA-Z_][a-zA-Z0-9_]*
INT_LIT  ::= ('0x' hex_digit+ | '0b' bin_digit+ | digit+) int_suffix?
FLOAT_LIT::= digit+ '.' digit+ float_suffix?
STR_LIT  ::= '"' char* '"'
BOOL_LIT ::= 'true' | 'false'

int_suffix   ::= 'i8'|'i16'|'i32'|'i64'|'u8'|'u16'|'u32'|'u64'
float_suffix ::= 'f32' | 'f64'
```

## Программа

```
program ::= decl*
```

## Объявления

```
decl ::= fn_decl
       | struct_decl
       | impl_decl
       | interface_decl
       | type_alias_decl
       | namespace_decl

fn_decl ::= 'pub'? 'fn' IDENT '(' param_list ')' ('->' type)? block

param_list ::= (param (',' param)*)?
param      ::= IDENT ':' type

struct_decl ::= 'struct' IDENT '{' (field_decl (',' field_decl)*)? '}'
field_decl  ::= 'pub'? IDENT ':' type

impl_decl ::= 'impl' IDENT (':' IDENT)? '{' fn_decl* '}'

interface_decl ::= 'interface' IDENT '{' method_sig* '}'
method_sig     ::= 'fn' IDENT '(' param_list ')' ('->' type)? ';'

type_alias_decl ::= 'type' IDENT '=' type ';'

namespace_decl ::= 'namespace' IDENT '{' decl* '}'
```

## Операторы

```
stmt ::= var_decl_stmt
       | expr_stmt
       | if_stmt
       | while_stmt
       | return_stmt
       | break_stmt
       | continue_stmt
       | block

var_decl_stmt ::= 'let' 'mut'? IDENT (':' type)? '=' expr ';'

expr_stmt ::= expr ';'

if_stmt ::= 'if' expr block ('else' (if_stmt | block))?

while_stmt ::= 'while' expr block

return_stmt   ::= 'return' expr? ';'
break_stmt    ::= 'break' ';'
continue_stmt ::= 'continue' ';'

block ::= '{' stmt* '}'
```

## Выражения (по приоритету, от низкого к высокому)

```
expr      ::= assign_expr
assign_expr ::= or_expr ('=' assign_expr)?
or_expr   ::= and_expr ('||' and_expr)*
and_expr  ::= eq_expr ('&&' eq_expr)*
eq_expr   ::= rel_expr (('=='|'!=') rel_expr)*
rel_expr  ::= add_expr (('<'|'>'|'<='|'>=') add_expr)*
add_expr  ::= mul_expr (('+'|'-') mul_expr)*
mul_expr  ::= unary_expr (('*'|'/'|'%') unary_expr)*
unary_expr::= ('-'|'!') unary_expr | postfix_expr
postfix_expr ::= primary_expr
               | postfix_expr '[' expr ']'
               | postfix_expr '.' IDENT
               | postfix_expr '.' IDENT '(' arg_list ')'
               | postfix_expr 'as' type
               | IDENT '(' arg_list ')'

primary_expr ::= INT_LIT | FLOAT_LIT | STR_LIT | BOOL_LIT
               | IDENT
               | IDENT '{' field_init (',' field_init)* '}'
               | '[' (expr (',' expr)*)? ']'
               | '(' expr ')'

arg_list   ::= (expr (',' expr)*)?
field_init ::= IDENT ':' expr
```

## Типы

```
type ::= IDENT
       | '[' type ';' INT_LIT ']'
       | 'void'
```

## Зарезервированные ключевые слова

`fn`, `let`, `mut`, `struct`, `type`, `namespace`, `impl`, `interface`,
`pub`, `priv`, `return`, `if`, `else`, `while`, `break`, `continue`,
`as`, `true`, `false`, `void`
