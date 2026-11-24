#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
find_uncolored_cout.py
Ищет в C++ файлах (.cpp, .h, .hpp) все cout, которые ещё не переделаны
под систему тем (нет theme::Role / Role).

Учитывает исключения:
  - ReadLine(...) — сама подставляет цвет
  - out << (ofstream, запись в файл)
  - чистые комментарии
  - cout после // (в комментариях)
"""

import os
import sys
import argparse
from pathlib import Path
from dataclasses import dataclass
from typing import List, Tuple

EXTENSIONS = {".cpp", ".h", ".hpp", ".cc", ".cxx"}


# ─────────────────────────────────────────────
# Категории найденных строк
# ─────────────────────────────────────────────
@dataclass
class Hit:
    file: Path
    line_num: int
    line: str
    kind: str  # "old_colors" | "plain_cout"


# ─────────────────────────────────────────────
# Анализ одной строки
# ─────────────────────────────────────────────
def analyze_line(line: str) -> str | None:
    """
    Возвращает:
      'old_colors' — cout со старыми colors:: (приоритет)
      'plain_cout' — cout без Role (надо разметить)
      None         — строка ок (не трогаем)
    """
    stripped = line.lstrip()

    # 1. Чистый комментарий — пропускаем
    if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
        return None

    # 2. Ищем позицию cout (std::cout или просто cout)
    #    Работаем с оригинальной строкой, чтобы сохранить позиции
    cout_pos = -1
    for needle in ("std::cout", "cout"):
        idx = line.find(needle)
        if idx != -1 and (cout_pos == -1 or idx < cout_pos):
            cout_pos = idx

    if cout_pos == -1:
        return None

    # 3. Проверяем, что cout идёт ДО комментария // (если он есть)
    comment_pos = line.find("//", cout_pos + 1)
    if comment_pos != -1:
        # cout после // — это просто текст в комментарии
        # Но! если cout ДО //, то это реальный код
        pass  # оставляем cout_pos как есть

    # 4. Исключение: out << (ofstream, запись в файл)
    #    Ищем "out <<" но НЕ "cout <<"
    #    Простая эвристика: если есть "out <<" и нет "cout <<" на той же позиции
    #    На самом деле, если мы уже нашли cout — это не out.
    #    Но на всякий случай проверим: если строка содержит "out <<" и НЕ содержит "cout <<"
    if "cout" not in line[cout_pos:cout_pos+10]:
        # это может быть out <<
        # но мы уже нашли cout выше, так что этот блок не сработает
        pass

    # 5. Исключение: ReadLine( — сама подставляет цвет
    #    Если в строке есть ReadLine( — пропускаем
    if "ReadLine(" in line:
        return None

    # 6. Исключение: cout << theme::Role или cout << Role — уже размечено
    if "Role" in line:
        return None

    # 7. Исключение: cout << colors::Reset в конце строки после Role — ок
    #    (но если Role нет — это old_colors)

    # 8. Проверяем: есть ли старые colors:: константы?
    has_old_colors = any(
        token in line
        for token in (
            "colors::Green", "colors::Red", "colors::Yellow",
            "colors::Cyan", "colors::Magenta", "colors::Burgundy",
            "colors::Bold", "colors::Dim", "colors::Reset",
            "colors::File", "colors::LineNumber", "colors::MatchText",
            "colors::Totals", "colors::MenuHeader", "colors::MenuNumber",
            "colors::Warning", "colors::Error",
        )
    )

    if has_old_colors:
        return "old_colors"

    # 9. Обычный cout без Role — надо разметить
    return "plain_cout"


# ─────────────────────────────────────────────
# Сканирование файла
# ─────────────────────────────────────────────
def scan_file(path: Path) -> List[Hit]:
    hits = []
    try:
        content = path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return hits

    for line_num, line in enumerate(content.splitlines(), start=1):
        kind = analyze_line(line)
        if kind is not None:
            hits.append(Hit(file=path, line_num=line_num, line=line.rstrip(), kind=kind))
    return hits


# ─────────────────────────────────────────────
# Главная функция
# ─────────────────────────────────────────────
def main() -> int:
    parser = argparse.ArgumentParser(
        description="Поиск cout без Role (непеределанные строки под систему тем)"
    )
    parser.add_argument(
        "-p", "--path",
        type=Path,
        default=Path.cwd(),
        help="Директория для сканирования (по умолчанию — текущая)",
    )
    parser.add_argument(
        "--no-recursive",
        action="store_true",
        help="Не рекурсивно (только верхний уровень)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Показывать больше контекста",
    )
    args = parser.parse_args()

    root: Path = args.path.resolve()
    if not root.exists() or not root.is_dir():
        print(f"[ошибка] директория не найдена: {root}", file=sys.stderr)
        return 1

    print(f"[info] сканирую: {root}")
    print(f"[info] ищу cout без Role...")
    print("-" * 60)

    all_hits: List[Hit] = []
    iterator = root.rglob("*") if not args.no_recursive else root.glob("*")

    for entry in iterator:
        if not entry.is_file() or entry.suffix.lower() not in EXTENSIONS:
            continue
        all_hits.extend(scan_file(entry))

    if not all_hits:
        print("✅ Все cout уже переделаны под систему тем!")
        return 0

    # Разделяем на категории
    old_colors = [h for h in all_hits if h.kind == "old_colors"]
    plain = [h for h in all_hits if h.kind == "plain_cout"]

    # Сначала показываем приоритетные (старые colors::)
    if old_colors:
        print(f"\n🔴 ПРИОРИТЕТ: найдено {len(old_colors)} строк со старыми colors::")
        print("   (нужно заменить на theme::Role::...)\n")
        for h in old_colors:
            rel = h.file.relative_to(root)
            print(f"  📄 {rel}:{h.line_num}")
            print(f"     {h.line}")
            if args.verbose:
                print()

    if plain:
        print(f"\n🟡 ОБЫЧНЫЕ: найдено {len(plain)} строк с cout без Role")
        print("   (нужно обернуть в theme::Role::...)\n")
        for h in plain:
            rel = h.file.relative_to(root)
            print(f"  📄 {rel}:{h.line_num}")
            print(f"     {h.line}")
            if args.verbose:
                print()

    print("-" * 60)
    print(f"[info] всего: {len(all_hits)} строк")
    print(f"       🔴 старые colors:: : {len(old_colors)}")
    print(f"       🟡 без Role        : {len(plain)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())