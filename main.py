#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Демонстрация раскраски вывода CodeSearch."""

RESET   = "\033[0m"
BOLD    = "\033[1m"
DIM     = "\033[2m"
RED     = "\033[31m"
GREEN   = "\033[32m"
YELLOW  = "\033[33m"
BLUE    = "\033[34m"
MAGENTA = "\033[35m"
CYAN    = "\033[36m"

def paint(text, *codes):
    return "".join(codes) + str(text) + RESET

def title(t):  return paint(t, BOLD, CYAN)   # заголовки === ... ===
def q(t):      return paint(t, CYAN)          # вопросы/подписи меню
def num(t):    return paint(t, YELLOW)        # цифра и точка в меню
def item(t):   return paint(t, GREEN)         # текст пункта меню
def expr(t):   return paint(t, MAGENTA)       # выражение / совпадение
def file(t):   return paint(t, BLUE)          # имя файла
def line(t):   return paint(t, RED)           # номер строки
def time(t):   return paint(t, DIM)           # время в истории буфера


# ─── Главное меню ───────────────────────────────────────────
print(title("=== Умный поиск и генератор regex ==="))
print()
print(q("Что искать?"))
print(f"  {num('1.')} {item('Структура')}")
print(f"  {num('2.')} {item('Комбинирование')}")
print(f"  {num('3.')} {item('Количество')}")
print(f"  {num('4.')} {item('Формат (email, URL, IP, GUID, дата)')}")
print(f"  {num('5.')} {item('Комментарии')}")
print(f"  {num('6.')} {item('Слово с границами')}")
print(f"  {num('7.')} {item('Избранное')}")
print(f"  {num('8.')} {item('История поисков')}")
print(f"  {num('9.')} {item('История буфера')}")
print(f"  {num('0.')} {item('Настройки')}")
print("> 1")
print()

# ─── Структура ──────────────────────────────────────────────
print(q("Что искать?"))
print(f"  {num('1.')} {item('X, внутри которого пусто')}")
print(f"  {num('2.')} {item('X, потом что угодно, потом Y, потом что угодно, потом Z')}")
print(f"  {num('0.')} {item('Назад')}")
print("> 1")
print()

print(q("Выберите параметр:"))
print(f"  {num('1.')} {item('Строго пусто')}")
print(f"  {num('2.')} {item('Не более N символов без пробелов')}")
print(f"  {num('3.')} {item('Не более N строк')}")
print(f"  {num('0.')} {item('Назад')}")
print("> 1")
print()

print(f"{q('Название оператора (0 — назад):')} if")
print()

print(q("Ваше выражение:"))
print(f"  {expr('[empty-block: if, строго пусто]')}")
print()

# ─── Меню действий ──────────────────────────────────────────
print(q("Выберите действие:"))
print(f"  {num('1.')} {item('Изменить условия')}")
print(f"  {num('2.')} {item('Скопировать в буфер')}")
print(f"  {num('3.')} {item('Начать поиск')}")
print(f"  {num('4.')} {item('Избранное')}")
print(f"  {num('5.')} {item('Добавить в избранное')}")
print(f"  {num('6.')} {item('История поисков')}")
print(f"  {num('7.')} {item('История буфера')}")
print(f"  {num('8.')} {item('Настройки')}")
print("> 3")
print()

# ─── Параметры поиска ───────────────────────────────────────
print(q("Измените параметры поиска:"))
print(f"  {num('1.')} {item('Начать поиск')}")
print(f"  {num('2.')} {item('Директория:')} [текущая]")
print(f"  {num('3.')} {item('Расширения:')} .cs")
print(f"  {num('4.')} {item('Исключённые папки:')} bin, obj, .git, .vs")
print(f"  {num('5.')} {item('Исключённые файлы:')} [пусто]")
print(f"  {num('6.')} {item('Максимальный размер:')} До 10 МБ")
print(f"  {num('0.')} {item('Отмена')}")
print("> 1")
print()

# ─── Результаты ─────────────────────────────────────────────
print("Поиск...")
print()
print(f"{q('Найдено файлов:')} 1")
print()
print(f"{file('dsfdfd.cs')}:{line('57')}: {expr('if (true)')}")
print()
print(f"{q('Найдено в файлах:')} 1")
print(f"{q('Всего найдено:')} 1")
print()

# ─── Меню действий после поиска ─────────────────────────────
print(q("Выберите действие:"))
print(f"  {num('1.')} {item('Изменить условия')}")
print(f"  {num('2.')} {item('Скопировать в буфер')}")
print(f"  {num('3.')} {item('Начать поиск')}")
print(f"  {num('4.')} {item('Избранное')}")
print(f"  {num('5.')} {item('Добавить в избранное')}")
print(f"  {num('6.')} {item('История поисков')}")
print(f"  {num('7.')} {item('История буфера')}")
print(f"  {num('8.')} {item('Настройки')}")
print(f"  {num('9.')} {item('Выписать в файл')}")
print("> 2")
print()

print(q("(скопировано в буфер)"))
print()

# ─── Возврат в главное меню ─────────────────────────────────
print(q("Выберите действие:"))
print("  ...")
print("> 1")
print()

print(title("=== Умный поиск и генератор regex ==="))
print()
print(q("Что искать?"))
print(f"  {num('1.')} {item('Структура')}")
print(f"  {num('2.')} {item('Комбинирование')}")
print(f"  {num('3.')} {item('Количество')}")
print(f"  {num('4.')} {item('Формат (email, URL, IP, GUID, дата)')}")
print(f"  {num('5.')} {item('Комментарии')}")
print(f"  {num('6.')} {item('Слово с границами')}")
print(f"  {num('7.')} {item('Избранное')}")
print(f"  {num('8.')} {item('История поисков')}")
print(f"  {num('9.')} {item('История буфера')}")
print(f"  {num('0.')} {item('Настройки')}")
print("> 7")
print()

# ─── История буфера ─────────────────────────────────────────
print(title("=== ИСТОРИЯ БУФЕРА ==="))
print()
print(f"  {num('1.')} {time('[2026-11-19 14:23:01]')} {time('(before_copy)')}")
print(f"     {item('[empty-block: if, строго пусто]')}")
print()
print(f"{q('Восстановить в буфер? (номер, 0 — назад):')} 0")
print()
print(q("Что искать?"))
print("  ...")
print(">")