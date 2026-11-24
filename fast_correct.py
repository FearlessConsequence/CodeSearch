import os

def find_cout_prompts_in_project():
    """
    Ищет во всех .cpp файлах строки, содержащие '>' и 'std::cout' одновременно.
    Выводит путь к файлу, номер строки и её содержимое.
    """
    current_dir = os.getcwd()
    found_items = []

    print(f"Поиск в директории: {current_dir}\n")

    for root, dirs, files in os.walk(current_dir):
        for file in files:
            if file.endswith('.cpp'):
                file_path = os.path.join(root, file)
                
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        lines = f.readlines()
                except UnicodeDecodeError:
                    try:
                        with open(file_path, 'r', encoding='cp1251') as f:
                            lines = f.readlines()
                    except Exception:
                        continue

                for i, line in enumerate(lines, 1):
                    # Условие: в строке есть и 'std::cout', и символ '>'
                    if 'std::cout' in line and '>' in line:
                        # Дополнительная проверка: отсекаем операторы сравнения (>>, >=)
                        # и стрелки шаблонов (SomeClass<SomeType>), оставляя только приглашение ' >'
                        # Ищем ' >' (пробел перед знаком) или '>' в конце строки перед ;
                        if ' >' in line or re.search(r'>\s*;', line):
                            # Убираем лишние пробелы и переносы для красивого вывода
                            clean_line = line.strip().replace('\n', ' ')
                            found_items.append({
                                'file': file_path,
                                'line': i,
                                'code': clean_line
                            })

    if not found_items:
        print("Совпадений не найдено.")
        return

    print(f"Найдено {len(found_items)} совпадений:\n")
    for item in found_items:
        print(f"Файл: {item['file']}")
        print(f"Строка: {item['line']}")
        print(f"Код:    {item['code']}\n")

# Для работы дополнительной проверки нужно импортировать re
import re

if __name__ == '__main__':
    find_cout_prompts_in_project()