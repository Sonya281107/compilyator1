# Кодогенерация компилятора Ryst

Компилятор ryc генерирует x86-64 assembly в синтаксисе NASM, затем вызывает `nasm` и `gcc` для получения исполняемого файла.

## Общая схема

```
source.ryst
    ↓ Lexer
tokens
    ↓ Parser
AST + SemTypePool
    ↓ Semantic Analyser
annotated AST (sem_type, frame_offset, resolved_fn, ...)
    ↓ Codegen
source.ryst.asm  (NASM x86-64)
    ↓ nasm -f elf64
source.ryst.o
    ↓ gcc -no-pie
executable
```

## Соглашение о вызовах — System V AMD64 ABI

| Регистр        | Роль                                 |
|----------------|--------------------------------------|
| `rdi rsi rdx rcx r8 r9` | Первые 6 целочисленных аргументов |
| `xmm0..xmm7`  | Первые 8 вещественных аргументов     |
| `rax`          | Целочисленное возвращаемое значение  |
| `xmm0`         | Вещественное возвращаемое значение   |
| `r12 r13 r14 r15` | Callee-saved (сохраняет вызываемый) |
| `r10 r11`      | Caller-saved, используются как temp  |
| `rbx`          | Caller-saved, temp для mul/div       |

## Распределение регистров (B.3.6)

Для каждой функции выполняется **раскраска графа интерференций**:

1. Сбор интервалов жизни переменных (live intervals)
2. Построение графа: ребро между переменными, чьи интервалы пересекаются
3. Жадная раскраска: вершины посещаются по убыванию степени, первый доступный цвет из `{r12, r13, r14, r15}`
4. Переменные без цвета (spill) размещаются на стеке как `[rbp − N]`

## Формат стекового кадра

```
  rbp      ← base pointer
  rbp - 8  ← первая локальная переменная (или регистровый spill)
  rbp - 16 ← вторая ...
  ...
  rbp - N  ← последняя локальная переменная
```

Кадр выравнивается до 16 байт: `sub rsp, N` (N кратно 16). Перед вызовом: `and rsp, -16`.

## Хранение типов

- **i8/i16/i32/i64, u8/u16/u32/u64, bool**: 8 байт на стеке (или в регистре)
- **f32/f64**: 8 байт на стеке (хранятся как double; `f32` расширяется до 64-бит внутри компилятора)
- **Строка**: 8 байт — указатель на null-terminated данные в `.rodata`
- **Массив `[T; N]`**: `N * 8` байт непрерывно на стеке
- **Структура**: сумма размеров полей (каждое 8 байт) на стеке
- **Интерфейсный fat pointer**: 16 байт = указатель данных + указатель vtable

## Секции output-файла

```
section .note.GNU-stack   ; стек без исполнения
section .rodata           ; константы, строки, vtable
section .bss              ; __input_buf
section .text             ; код
```

## Vtable и динамический диспатч (A.2.13)

### Структура fat pointer

```asm
[rbp - N]     ; data_ptr  — адрес конкретного объекта
[rbp - N + 8] ; vtable_ptr — адрес vtable
```

### Vtable в .rodata

```asm
__vtable_Circle_Shape:
    dq __thunk_Circle_Shape_area
    dq __thunk_Circle_Shape_perimeter
```

### Thunk-обёртки

Метод `impl S { fn m(self: S) }` получает поля структуры в регистрах (`rdi=field0, rsi=field1, …`). Vtable-диспатч передаёт `rdi=data_ptr`. Thunk конвертирует:

```asm
__thunk_Circle_Shape_area:
    mov r11, rdi            ; r11 = data_ptr
    mov rdi, [r11 + 0]     ; field 0 (например, radius)
    jmp Circle__area__Circle
```

### Инициализация fat pointer

```asm
; let s: Shape = circle_var;
lea rax, [rbp - 16]          ; &circle_var
mov [rbp - 32], rax           ; data_ptr
lea rax, [rel __vtable_Circle_Shape]
mov [rbp - 24], rax           ; vtable_ptr
```

### Вызов через vtable

```asm
; s.area()  →  emit_vtable_call
mov rdi, [rbp - 32]           ; data_ptr → rdi (self)
and rsp, -16
mov r10, [rbp - 24]           ; vtable_ptr
call [r10 + 0]                ; slot 0 = area
```

## Встроенные функции

Реализованы как специальные пути в `emit_call` (без настоящего вызова):

| Имя               | Действие                            |
|-------------------|-------------------------------------|
| `__builtin_print` | printf с форматом по типу аргумента |
| `__builtin_input` | системный вызов read + __input_buf  |
| `__builtin_exit`  | вызов libc exit()                   |
| `__builtin_panic` | printf + exit(1)                    |
| `__builtin_print_char` | вызов putchar                  |

## Оператор as (явное приведение)

| Направление       | Инструкция             |
|-------------------|------------------------|
| int → float       | `cvtsi2sd xmm0, rax`   |
| float → int       | `cvttsd2si rax, xmm0`  |
| int → wider int   | `movsx` / `movzx`      |
| int → narrower int| `and rax, 0xFF` и т.п. |
