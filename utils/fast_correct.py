import re
import os

def find_missing_hex_values():
    # Список всех искомых значений (очищенный от названий групп)
    target_values = [
        "aeb7b7", "h2h2h2", "ff875f", "87afaf", "dadada",
        "ffaf5f", "005f5f", "008787", "870000", "585858", "af005f",
        "8787d7", "5f8787", "00875f", "5f8700", "5fafff",
        "af5faf", "afd7d7", "hchchc", "87afaf", "afd7af",
        "5f87af", "8787af", "af87af", "d7af87",
        "87875f", "eeeeee", "8787af",
        "005f87", "0087af", "87ffff", "5fafff",
        "87afd7", "ffd7ff", "ffffd7", "ffd7af",
        "87d787", "afd7d7", "ffd75f", "5f87d7",
        "87878af", "87875f",
        "00d7af", "ffd787",
        "5f5f00", "875f00", "af5f00", "d7875f",
        "5faf5f", "00afff"
    ]

    # Приводим к нижнему регистру для регистронезависимого поиска
    found_values = set()
    target_set = set(v.lower() for v in target_values)

    file_path = "colors.txt"

    if not os.path.exists(file_path):
        print(f"Файл {file_path} не найден в текущей директории.")
        return

    # НОВОЕ РЕГУЛЯРНОЕ ВЫРАЖЕНИЕ: ищем именно "HEX: #XXXXXX"
    hex_pattern = re.compile(r'HEX:\s*#([a-f0-9]{6})', re.IGNORECASE)

    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            matches = hex_pattern.findall(line)
            for match in matches:
                found_values.add(match.lower())

    # Вычисляем разницу
    missing_values = sorted(list(target_set - found_values))

    print(f"Проверено значений: {len(target_values)}")
    print(f"Найдено в файле: {len(found_values)}")
    
    if not missing_values:
        print("\nВсе значения найдены в файле!")
    else:
        print(f"\nНе найдено {len(missing_values)} значений:")
        for val in missing_values:
            print(f"- {val}")

if __name__ == '__main__':
    find_missing_hex_values()