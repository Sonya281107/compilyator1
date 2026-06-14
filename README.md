# Ryst

Статически типизированный учебный язык программирования. Компилятор **ryc** транслирует `.ryst`-файл в нативный x86-64 ELF-исполняемый файл.

```
source.ryst → Lexer → Parser → Semantic → Codegen → NASM → GCC → ELF
```

---

## Установка

### 1. Зависимости

Убедитесь, что установлены следующие пакеты:

| Инструмент | Версия | Установка (Ubuntu/Debian) |
|------------|--------|---------------------------|
| clang++    | ≥ 18   | `sudo apt install clang` |
| libc++     | ≥ 18   | `sudo apt install libc++-18-dev libc++abi-18-dev` |
| CMake      | ≥ 3.30 | `sudo apt install cmake` или [cmake.org](https://cmake.org/download/) |
| Ninja      | любая  | `sudo apt install ninja-build` |
| NASM       | ≥ 2.14 | `sudo apt install nasm` |
| GCC        | любая  | `sudo apt install gcc` |

Проверка наличия нужных версий:

```bash
clang++ --version   # должно быть >= 18
cmake --version     # должно быть >= 3.30
nasm --version
```

### 2. Клонирование репозитория

```bash
git clone https://github.com/Sonya281107/compilyator1
cd compilyator
```

### 3. Сборка компилятора

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

После этого бинарник компилятора находится в `build/ryc`.

### 4. (Опционально) Установка в систему

```bash
sudo cp build/ryc /usr/local/bin/ryc
```

После этого `ryc` доступен глобально без указания пути.

---

## Использование

```bash
# Скомпилировать программу и создать исполняемый файл
ryc path/to/program.ryst -o my_program

# Запустить результат
./my_program
```

### Пример

```bash
ryc examples/hello.ryst -o hello
./hello
```

### Дополнительные флаги

```bash
ryc program.ryst --dump-tokens   # вывести токены и выйти
ryc program.ryst --dump-ast      # вывести AST и выйти
```

---

## Быстрый старт

Создайте файл `hello.ryst`:

```ryst
fn main() -> i64 {
    print("Hello, World!");
    return 0;
}
```

Скомпилируйте и запустите:

```bash
ryc hello.ryst -o hello
./hello
# Hello, World!
```

## Сборка с санитайзерами

Для отладки компилятора можно включить AddressSanitizer и UBSanitizer:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build
```

---

## Спецификация языка

- [specs/grammar.md](specs/grammar.md) — формальная грамматика (EBNF)
- [specs/semantics.md](specs/semantics.md) — семантические правила
- [specs/types.md](specs/types.md) — система типов
- [specs/codegen.md](specs/codegen.md) — устройство кодогенератора
