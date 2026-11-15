// ============================================================
// ТЕСТОВЫЙ ФАЙЛ для CodeSearch (C++)
// Проверка всех 6 типов поиска
// ============================================================

using System;
using System.Collections.Generic;
using System.Linq.Expressions;
using System.Text.RegularExpressions;

namespace TestProject
{
    // ============================================================
    // ТИП 1. СЛОВО С ГРАНИЦАМИ
    // ============================================================

    public class UserService
    {
        public string username = "admin";          // "name" внутри username
        public string Name = "Test";                // регистр другой
        public string filename = "test.txt";        // внутри filename

        public void SetName(string name)            // ← цель
        {
            var nameofVar = nameof(name);           // внутри nameof
            Console.WriteLine(name);                // и этот "name"
        }

        public void Save() { }
        public void SaveChanges() { }
        public void AutoSave() { }
        public void SaveAll() { }
    }

    // ============================================================
    // ТИП 2. ФОРМАТ
    // ============================================================

    public class DataExamples
    {
        // Email
        public string email1 = "admin@company.com";
        public string email2 = "help@mail.ru";
        public string email3 = "developer@yandex-team.ru";
        public string notEmail = "@no-user.com";

        // URL
        public string url1 = "https://api.example.com/v1/users";
        public string url2 = "http://docs.local/page";
        public string url3 = "https://localhost:5000";
        public string notUrl = "ftp://files.example.com";

        // IP
        public string ip1 = "192.168.1.1";
        public string ip2 = "127.0.0.1";
        public string ip3 = "10.0.0.1";
        public string badIp = "192.168.1.1.1";

        // GUID
        public string guid1 = "550e8400-e29b-41d4-a716-446655440000";
        public string guid2 = "550E8400-E29B-41D4-A716-446655440000";
        public string guidBad = "550e8400-e29b-41d4-a716-446655440000-extra";

        // Дата
        public string date1 = "2024-05-12";
        public string date2 = "2023-01-01";
        public string date3 = "12.05.2024";
        public string date4 = "2024/05/12";
        public string dateBad = "2024-5-1";
    }

    // ============================================================
    // ТИП 3. СТРУКТУРА
    // ============================================================

    // 3.1. Пустой блок

    public class EmptyBlocks
    {
        public void TestCatch()
        {
            try
            {
            }
            catch { }                                  // ← пустой catch

            try
            {
            }
            catch (Exception) { }                      // ← пустой catch с Exception

            try
            {
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);          // ← НЕ пустой
            }
        }

        public void EmptyMethod() { }

        public void EmptyMethodMulti()
        {
        }

        public class EmptyClass { }
    }

    // 3.2. X, потом Y, потом Z

    public class Order
    {
        public int total;
        public string customer;

        public void Calculate()
        {
            total = 100;
        }
    }

    public class BigClass
    {
        public int a;

        public void Method1()
        {
            if (true)
            {
            }
        }

        public void Method2()
        {
            var str = "const string Green";
        }

        public int b;
        public string c;

        public void Method3()
        {
            while (true)
            {
                break;
            }
        }
    }

    // ============================================================
    // ТИП 4. КОМБИНИРОВАНИЕ
    // ============================================================

    public class AsyncTests
    {
        public async Task DoWorkAsync() { }
        public Task ProcessOrder() { return null; }
        public async Task<int> GetValueAsync() { return 0; }
        public Task<int> GetUserId() { return null; }
        public Task<string> GetName() { return null; }
    }

    public class Comments
    {
        // TODO: заменить API
        // FIXME: падает при пустом вводе
        // HACK: временное решение
        // NOTE: не исправлять

        public void Work() { }
    }

    // ============================================================
    // ТИП 5. КОЛИЧЕСТВО
    // ============================================================

    public class Numbers
    {
        public int a = 5;
        public int b = 42;
        public int c = 999;
        public int d = 1000;
        public int e = 12345;
        public int f = 5000000;
        public int year = 2024;
        public int phone = 12345678901;
    }

    public class LongLines
    {
        public string shortLine = "ok";
        public string mediumLine = "средняя строка";
        public string longLine = "очень длинная строка, которая точно должна найтись при поиске длинных строк больше 50 символов";
        public string veryLongLine = "эта строка ещё длиннее, она должна найтись при поиске строк длиннее 50 символов и показать пользователю, что в коде есть проблема со стилем";
    }

    public class Args
    {
        public void A(int x) { }
        public void B(int x, int y) { }
        public void C(int x, int y, int z) { }
        public void D(int x, int y, int z, string a) { }
        public void E(int x, int y, int z, string a, string b, string c) { }
    }

    // ============================================================
    // ТИП 6. КОММЕНТАРИИ
    // ============================================================

    public class CommentTests
    {
        try
        {
            
        }

        catch
        {
            
        }
        catch (System.Exception)
        {
            
            throw;
        }
        // TODO: доработать
        // FIXME: сломано
        // HACK: обходной путь

        public int x = 5; // комментарий в конце строки
        public int y = 10; // ещё один

        // public void OldMethod() { }
        // var x = 5;
        // this.Main().Show();

        // Это обычный текстовый комментарий
        // А это ещё один

        public void Work()

        for (){

            
        }
        else {


        }

        if()
        {

        }
        public static void Work()
        {
            // внутренний комментарий
            int z = 5; // в конце
            // int z = 5;

        }

    }
}



// ============================================================
// ТЕСТЫ для "X, но не Y"
// ============================================================

// ============================================================
// 1.1. По позиции — X не является частью Y
// ============================================================

public class TestByPosition
{
    // X = "catch", Y = "catch (Exception)"
    // Ожидание: строки 2 и 3 находим, строку 1 — НЕТ
    public void TestCatch()
    {
        try { }
        catch (Exception ex) { }        // ← НЕ находим (catch внутри "catch (Exception)")
        catch { }                       // ← находим (catch не пересекается с Y)
        catch (MyError) { }             // ← находим (catch не пересекается с Y)
    }

    // X = "foo", Y = "bar"
    // Ожидание: строку 1 находим
    public void TestFoo()
    {
        foo("bar");                     // ← находим (foo и bar на разных позициях)
        barbar;                         // ← foo нет
    }

    // X = "if", Y = "while"
    // Ожидание: обе строки с if находим
    public void TestIfWhile()
    {
        if (x) { }                      // ← находим (while нет рядом)
        while (y) { if (z) { } }        // ← находим if (не пересекается с while)
    }
}

// ============================================================
// 1.2. По строке — в строке с X нет Y
// ============================================================

public class TestByLine
{
    // X = "if", Y = "Console"
    // Ожидание: строку 1 НЕ находим, строки 2 и 3 — находим
    public void TestIfConsole()
    {
        if (x) { Console.WriteLine(); } // ← НЕ находим (Console в этой же строке)
        if (y) { }                       // ← находим (Console нет в строке)

        if (z) {                         // ← находим (Console на другой строке)
            Console.WriteLine();
        }
    }
}


if (x) { Console.WriteLine(); }   // ← НЕ находим (Console в строке)
if (y) { }                        // ← находим

if (z) {                          // ← находим (Console на другой строке)
    Console.WriteLine();
}




void Test() {
    SomeMethod(1, 2);                  // → [call]
    var x = new Foo(1, 2);             // → [call]
    Console.WriteLine(a, b);           // → [call]
    OtherClass.Helper(p, q);           // → [call]
}


// ============================================================
// ТЕСТ 1. Строчные комментарии в начале строки
// ============================================================

// TODO: это в начале — найдётся в режиме "начало"
// FIXME: это тоже начало — найдётся
    // HACK: с отступом, но всё равно начало — найдётся
	// NOTE: с табом — найдётся

public class A
{
    // TODO: внутри класса, начало строки — найдётся
    int x = 5;
}

// ============================================================
// ТЕСТ 2. Строчные комментарии в конце строки
// ============================================================

public class B
{
    int a = 1; // TODO: в конце — найдётся в режиме "конец"
    int b = 2; // FIXME: тоже конец — найдётся
    string s = "x"; // HACK: и это конец — найдётся

    // а это начало, не конец — в режиме "конец" НЕ найдётся
}

// ============================================================
// ТЕСТ 3. Блочные комментарии /* */ — одна строка
// ============================================================

/* TODO: блочный в начале строки — найдётся в режиме "начало" */
public class C
{
    int x = 5; /* FIXME: блочный в конце строки — найдётся в режиме "конец" */
    /* HACK: блочный внутри класса, начало — найдётся */
}

// ============================================================
// ТЕСТ 4. Многострочные блочные комментарии
// ============================================================

/* TODO: начало блока
   продолжение блока
   конец блока */

/*
 * FIXME: звёздочка в начале
 * ещё строка
 */

public class D
{
    int y = 10; /* HACK: начало блока в конце строки
                    вторая строка блока
                    конец */ 
}

// ============================================================
// ТЕСТ 5. Ловушки — // внутри строк НЕ комментарий
// ============================================================

public class E
{
    string url1 = "http://example.com";           // это URL, не комментарий
    string url2 = "https://test.ru/path";         // тоже URL
    string path  = "C://Windows//System32";       // и это
    string s     = "// TODO: это в строке, не комментарий";

    string real = "x"; // TODO: А ЭТО настоящий комментарий — найдётся
}

// ============================================================
// ТЕСТ 6. Ловушки — /* внутри строк НЕ комментарий
// ============================================================

public class F
{
    string pattern = "/* не комментарий */";
    string regex   = "a/*b";  // TODO: а вот это настоящий — найдётся
}

// ============================================================
// ТЕСТ 7. Слово в разных местах блока
// ============================================================

/* начало TODO середина конец */

/*
TODO: сразу после открытия — это всё равно блок
*/

/*
   много пробелов до TODO — всё равно блок
*/

/* TODO: перед закрытием — тоже блок */

// ============================================================
// ТЕСТ 8. Регистр и частичное совпадение
// ============================================================

// TODO: заглавные
// todo: строчные — при поиске "TODO" не найдётся (регистрозависимо)
// TODOO: лишняя буква — при поиске "TODO" НАЙДЁТСЯ (это подстрока)
// XTODO: буква перед — тоже найдётся как подстрока

// ============================================================
// ТЕСТ 9. Пустые и почти пустые
// ============================================================

//
// 
//   
/* */
/*  */
/**/
/**  **/

// ============================================================
// ТЕСТ 10. Комментарии с кодом после
// ============================================================

public class G
{
    void M1() { } // после метода
    void M2() { } /* после метода блочный */
    /* до метода */ void M3() { }
}

// ============================================================
// ТЕСТ 11. Комментарий после комментария в строке
// ============================================================

// TODO: первая строка
// FIXME: вторая строка
// HACK: третья строка

/* блочный */ /* ещё блочный */ /* TODO: третий */

// ============================================================
// ТЕСТ 12. Вложенные кавычки
// ============================================================

public class H
{
    string s1 = "он сказал \"// это не комментарий\"";
    string s2 = 'a'; // TODO: а это настоящий — найдётся
    char c1 = '"';   // FIXME: и это настоящий — найдётся
}

alice is lovely woman