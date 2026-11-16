#include <iostream>
#include <string>
#include <regex>
#include <fstream>
#include <filesystem>
#include <vector>
#include <ctime>
#include <cstdlib>

#include "RegexBuilder.h"
#include "FileSearcher.h"
#include "Settings.h"
#include "Utf8.h"

namespace fs = std::filesystem;

// ============================================================
// Мини-фреймворк
// ============================================================

static int g_passed = 0;
static int g_failed = 0;

#define CHECK_EQ(actual, expected) do { \
    auto _a = (actual); auto _e = (expected); \
    if (!(_a == _e)) { \
        std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ \
                  << "  expected [" << _e << "] got [" << _a << "]\n"; \
        ++g_failed; \
    } else { ++g_passed; } \
} while (0)

#define CHECK_TRUE(x)  CHECK_EQ(!!(x), true)
#define CHECK_FALSE(x) CHECK_EQ(!!(x), false)

static void Section(const char* name) { std::cout << "[" << name << "]\n"; }

// ============================================================
// Unit: Escape
// ============================================================

static void Test_Escape() {
    Section("Escape");
    using regex_builder::Escape;
    CHECK_EQ(Escape(""),         "");
    CHECK_EQ(Escape("abc"),      "abc");
    CHECK_EQ(Escape("a b"),      "a b");
    CHECK_EQ(Escape("a-b/c"),    "a-b/c");
    CHECK_EQ(Escape("a.b*c"),    "a\\.b\\*c");
    CHECK_EQ(Escape("a|b"),      "a\\|b");
    CHECK_EQ(Escape("a(b)"),     "a\\(b\\)");
    CHECK_EQ(Escape("a[b]"),     "a\\[b\\]");
    CHECK_EQ(Escape("a{b}"),     "a\\{b\\}");
    CHECK_EQ(Escape("a+b?"),     "a\\+b\\?");
    CHECK_EQ(Escape("a^b$"),     "a\\^b\\$");
    CHECK_EQ(Escape("a\\b"),     "a\\\\b");
}

// ============================================================
// Unit: EscapeXyz
// ============================================================

static void Test_EscapeXyz() {
    Section("EscapeXyz");
    using regex_builder::EscapeXyz;
    CHECK_EQ(EscapeXyz(""),        "");
    CHECK_EQ(EscapeXyz("abc"),     "abc");
    CHECK_EQ(EscapeXyz("a%b"),     "a\\s*b");
    CHECK_EQ(EscapeXyz("a*b"),     "a[\\s\\S]*?b");
    CHECK_EQ(EscapeXyz("^foo$"),   "^foo$");
    CHECK_EQ(EscapeXyz("a.b(c)"),  "a\\.b\\(c\\)");
    CHECK_EQ(EscapeXyz("foo % bar"), "foo \\s* bar");
    CHECK_EQ(EscapeXyz("%%"),      "\\s*\\s*");
    CHECK_EQ(EscapeXyz("**"),      "[\\s\\S]*?[\\s\\S]*?");
    CHECK_EQ(EscapeXyz("a^b"),     "a^b");
    CHECK_EQ(EscapeXyz("a$b"),     "a$b");
    CHECK_EQ(EscapeXyz("a|b"),     "a\\|b");
}

// ============================================================
// Unit: EscapeXyzWithBounds
// ============================================================

static void Test_EscapeXyzWithBounds() {
    Section("EscapeXyzWithBounds");
    using regex_builder::EscapeXyzWithBounds;
    // Простое слово — обе границы
    CHECK_EQ(EscapeXyzWithBounds("hello"),   "\\bhello\\b");
    // Ведущие/хвостовые пробелы
    CHECK_EQ(EscapeXyzWithBounds(" hello "), "\\bhello\\b");
    CHECK_EQ(EscapeXyzWithBounds("  foo  "), "\\bfoo\\b");
    CHECK_EQ(EscapeXyzWithBounds("\tfoo\n"), "\\bfoo\\b");
    // Не-словесные символы на границах — без \b с этой стороны
    CHECK_EQ(EscapeXyzWithBounds("---"),     "---");
    CHECK_EQ(EscapeXyzWithBounds(".foo"),    "\\.foo\\b");
    CHECK_EQ(EscapeXyzWithBounds("foo."),    "\\bfoo\\.");
    CHECK_EQ(EscapeXyzWithBounds("_foo"),    "\\b_foo\\b");
    CHECK_EQ(EscapeXyzWithBounds("123"),     "\\b123\\b");
    // Пустое / пробельное
    CHECK_EQ(EscapeXyzWithBounds(""),        "");
    CHECK_EQ(EscapeXyzWithBounds("   "),     "");
    CHECK_EQ(EscapeXyzWithBounds("\t\n"),    "");
    // Спецсимволы
    CHECK_EQ(EscapeXyzWithBounds("%foo"),    "\\s*foo\\b");
    CHECK_EQ(EscapeXyzWithBounds("foo%"),    "\\bfoo\\s*");
    CHECK_EQ(EscapeXyzWithBounds("*"),       "[\\s\\S]*?");
    CHECK_EQ(EscapeXyzWithBounds("^"),       "^");
    CHECK_EQ(EscapeXyzWithBounds("$"),       "$");
}

// ============================================================
// Unit: BuildWord
// ============================================================

static void Test_BuildWord() {
    Section("BuildWord");
    auto make = [](const std::string& w, bool whole) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Word;
        r.Word = w;
        r.WordWholeOnly = whole;
        return regex_builder::Build(r);
    };
    CHECK_EQ(make("hello", true),   "\\bhello\\b");
    CHECK_EQ(make("hello", false),  "hello");
    CHECK_EQ(make("a.b",   false),  "a\\.b");
    CHECK_EQ(make("a.b",   true),   "\\ba\\.b\\b");
    CHECK_EQ(make("_foo",  true),   "\\b_foo\\b");
    CHECK_EQ(make(".foo",  true),   "\\.foo\\b");
    CHECK_EQ(make("foo.",  true),   "\\bfoo\\.");
    CHECK_EQ(make("---",   true),   "---");
    CHECK_EQ(make("123",   true),   "\\b123\\b");
    CHECK_EQ(make("",      true),   "");
    CHECK_EQ(make("a(b)",  false),  "a\\(b\\)");
    CHECK_EQ(make("a(b)",  true),   "\\ba\\(b\\)");
}

// ============================================================
// Unit: BuildCombination (XOrY)
// ============================================================

static void Test_BuildCombination_XOrY() {
    Section("BuildCombination (XOrY)");
    auto make = [](const std::string& x) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Combination;
        r.Combination = regex_builder::CombinationType::XOrY;
        r.X = x;
        return regex_builder::Build(r);
    };
    CHECK_EQ(make("foo, bar"),    "(?:\\bfoo\\b|\\bbar\\b)");
    CHECK_EQ(make("foo"),         "(?:\\bfoo\\b)");
    CHECK_EQ(make("foo,bar,baz"), "(?:\\bfoo\\b|\\bbar\\b|\\bbaz\\b)");
    CHECK_EQ(make("foo,  bar "),  "(?:\\bfoo\\b|\\bbar\\b)");
    CHECK_EQ(make(""),            "");
    CHECK_EQ(make("  "),          "");
    CHECK_EQ(make("foo bar"),     "(?:\\bfoo bar\\b)");
    CHECK_EQ(make("a-b, c-d"),    "(?:\\ba-b\\b|\\bc-d\\b)");
}

// ============================================================
// Unit: BuildStructure (X→Y→Z)
// ============================================================

static void Test_BuildStructure_XYZ() {
    Section("BuildStructure (X->Y->Z)");
    auto make = [](const std::string& x, const std::string& y, const std::string& z) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Structure;
        r.Structure = regex_builder::StructureType::XAnywhereBeforeYBeforeZ;
        r.X = x; r.Y = y; r.Z = z;
        return regex_builder::Build(r);
    };
    CHECK_EQ(make("a","b","c"), "\\ba\\b[\\s\\S]*?(\\bb\\b)[\\s\\S]*?\\bc\\b");
    CHECK_EQ(make("foo","bar","baz"), "\\bfoo\\b[\\s\\S]*?(\\bbar\\b)[\\s\\S]*?\\bbaz\\b");
    CHECK_EQ(make("","b","c"), "");
    CHECK_EQ(make("a","","c"), "");
    CHECK_EQ(make("a","b",""), "");
    CHECK_EQ(make("","",""),   "");
    CHECK_EQ(make("foo bar","mid","end"), "\\bfoo bar\\b[\\s\\S]*?(\\bmid\\b)[\\s\\S]*?\\bend\\b");
}

// ============================================================
// Unit: BuildEmpty
// ============================================================

static void Test_BuildEmpty() {
    Section("BuildEmpty");
    auto make = [](const std::string& x, regex_builder::EmptyMetric m, int n) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Structure;
        r.Structure = regex_builder::StructureType::XEmpty;
        r.X = x;
        r.EmptyMetricValue = m;
        r.EmptyMaxCount = n;
        return regex_builder::Build(r);
    };
    using M = regex_builder::EmptyMetric;
    CHECK_EQ(make("if", M::StrictlyEmpty, 0), "[empty-block: if, строго пусто]");
    CHECK_EQ(make("if", M::MaxChars, 3),      "[empty-block: if, ≤ 3 непроб. символов]");
    CHECK_EQ(make("if", M::MaxLines, 2),      "[empty-block: if, ≤ 2 строк]");
    CHECK_EQ(make("catch", M::StrictlyEmpty, 0), "[empty-block: catch, строго пусто]");
    CHECK_EQ(make("while", M::MaxLines, 0),   "[empty-block: while, ≤ 0 строк]");
}

// ============================================================
// Unit: BuildComment
// ============================================================

static void Test_BuildComment() {
    Section("BuildComment");
    auto make = [](regex_builder::CommentType t, const std::string& lang, const std::string& w) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Comment;
        r.Comment = t;
        r.Language = lang;
        r.CommentWord = w;
        return regex_builder::Build(r);
    };
    using T = regex_builder::CommentType;
    CHECK_EQ(make(T::StartLine, "C#", ""),     "[comments: в начале строки, язык: C#, слово: (все)]");
    CHECK_EQ(make(T::EndLine, "C#", "TODO"),   "[comments: в конце строки, язык: C#, слово: TODO]");
    CHECK_EQ(make(T::CommentedCode, "C#", ""), "[comments: все комментарии, язык: C#, слово: (все)]");
    CHECK_EQ(make(T::StartLine, "Python", ""), "[comments: в начале строки, язык: Python, слово: (все)]");
    CHECK_EQ(make(T::EndLine, "C++", "FIX"),   "[comments: в конце строки, язык: C++, слово: FIX]");
}

// ============================================================
// Unit: BuildQuantity (Numbers)
// ============================================================

static bool RxHit(const std::string& pattern, const std::string& text) {
    try { return std::regex_search(text, std::regex(pattern)); }
    catch (...) { return false; }
}

static void Test_BuildQuantity_Numbers() {
    Section("BuildQuantity (Numbers)");
    auto make = [](regex_builder::NumberCompare c, int mn, int mx) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Quantity;
        r.Quantity = regex_builder::QuantityType::Numbers;
        r.NumberCompareValue = c;
        r.MinCount = mn; r.MaxCount = mx;
        return regex_builder::Build(r);
    };
    using NC = regex_builder::NumberCompare;

    // Equal
    std::string eq3 = make(NC::Equal, 3, 0);
    CHECK_TRUE (RxHit(eq3, "999"));
    CHECK_FALSE(RxHit(eq3, "99"));
    CHECK_FALSE(RxHit(eq3, "1000"));
    CHECK_FALSE(RxHit(eq3, "12a"));

    // Greater
    std::string gt3 = make(NC::Greater, 3, 0);
    CHECK_TRUE (RxHit(gt3, "1000"));
    CHECK_TRUE (RxHit(gt3, "12345"));
    CHECK_FALSE(RxHit(gt3, "999"));

    // Less
    std::string lt5 = make(NC::Less, 5, 0);
    CHECK_TRUE (RxHit(lt5, "1234"));
    CHECK_TRUE (RxHit(lt5, "1"));
    CHECK_FALSE(RxHit(lt5, "12345"));

    // Between
    std::string bt = make(NC::Between, 2, 4);
    CHECK_TRUE (RxHit(bt, "12"));
    CHECK_TRUE (RxHit(bt, "1234"));
    CHECK_FALSE(RxHit(bt, "1"));
    CHECK_FALSE(RxHit(bt, "12345"));
}

// ============================================================
// Unit: BuildQuantity (LineLength) — только string compare
// ============================================================

static void Test_BuildQuantity_LineLength() {
    Section("BuildQuantity (LineLength)");
    auto make = [](regex_builder::NumberCompare c, int mn, int mx) {
        regex_builder::SearchRequest r;
        r.Type = regex_builder::SearchType::Quantity;
        r.Quantity = regex_builder::QuantityType::LineLength;
        r.NumberCompareValue = c;
        r.MinCount = mn; r.MaxCount = mx;
        return regex_builder::Build(r);
    };
    using NC = regex_builder::NumberCompare;
    CHECK_EQ(make(NC::Greater, 5, 0),  "^.{6,}$");
    CHECK_EQ(make(NC::Equal,   5, 0),  "^.{5}$");
    CHECK_EQ(make(NC::Less,    5, 0),  "^.{1,4}$");
    CHECK_EQ(make(NC::Between, 5, 10), "^.{5,10}$");
    CHECK_EQ(make(NC::Greater, 50, 0), "^.{51,}$");
    CHECK_EQ(make(NC::Less,    1, 0),  "(?!)");
}

// ============================================================
// Unit: patterns::ForFormat
// ============================================================

static void Test_ForFormat() {
    Section("patterns::ForFormat");
    using patterns::FormatType;
    using patterns::ForFormat;

    std::string email = ForFormat(FormatType::Email);
    CHECK_TRUE (RxHit(email, "admin@company.com"));
    CHECK_TRUE (RxHit(email, "help@mail.ru"));
    CHECK_TRUE (RxHit(email, "a.b+c@x.y"));
    CHECK_FALSE(RxHit(email, "@no-user.com"));
    CHECK_FALSE(RxHit(email, "no-at-sign"));

    std::string url = ForFormat(FormatType::Url);
    CHECK_TRUE (RxHit(url, "https://api.example.com/v1/users"));
    CHECK_TRUE (RxHit(url, "http://docs.local/page"));
    CHECK_TRUE (RxHit(url, "https://localhost:5000"));
    CHECK_FALSE(RxHit(url, "ftp://files.example.com"));
    CHECK_FALSE(RxHit(url, "javascript:void(0)"));

    std::string ip = ForFormat(FormatType::Ip);
    CHECK_TRUE (RxHit(ip, "192.168.1.1"));
    CHECK_TRUE (RxHit(ip, "10.0.0.1"));
    CHECK_TRUE (RxHit(ip, "127.0.0.1"));
    CHECK_FALSE(RxHit(ip, "192.168.1.1.1"));
    CHECK_FALSE(RxHit(ip, "256.1.1.1"));

    std::string guid = ForFormat(FormatType::Guid);
    CHECK_TRUE (RxHit(guid, "550e8400-e29b-41d4-a716-446655440000"));
    CHECK_TRUE (RxHit(guid, "550E8400-E29B-41D4-A716-446655440000"));
    CHECK_FALSE(RxHit(guid, "550e8400-e29b-41d4-a716-446655440000-extra"));
    CHECK_FALSE(RxHit(guid, "not-a-guid"));

    std::string date = ForFormat(FormatType::Date);
    CHECK_TRUE (RxHit(date, "2024-05-12"));
    CHECK_TRUE (RxHit(date, "12.05.2024"));
    CHECK_TRUE (RxHit(date, "2024/05/12"));
    CHECK_TRUE (RxHit(date, "2023-01-01"));
    CHECK_FALSE(RxHit(date, "2024-5-1"));
    CHECK_FALSE(RxHit(date, "2024-13-01"));
    CHECK_FALSE(RxHit(date, "2024-05-32"));
}

// ============================================================
// Unit: utf8::SafeCut
// ============================================================

static void Test_SafeCut() {
    Section("utf8::SafeCut");
    using utf8::SafeCut;
    // ASCII — без изменений
    CHECK_EQ(SafeCut("abc", 0), (size_t)0);
    CHECK_EQ(SafeCut("abc", 1), (size_t)1);
    CHECK_EQ(SafeCut("abc", 2), (size_t)2);
    CHECK_EQ(SafeCut("abc", 3), (size_t)3);
    // "привет" — 6 букв, 12 байт
    std::string s = "привет";
    CHECK_EQ(s.size(), (size_t)12);
    CHECK_EQ(SafeCut(s, 0),  (size_t)0);
    CHECK_EQ(SafeCut(s, 1),  (size_t)0);   // середина первой буквы
    CHECK_EQ(SafeCut(s, 2),  (size_t)2);   // граница
    CHECK_EQ(SafeCut(s, 3),  (size_t)2);   // середина второй
    CHECK_EQ(SafeCut(s, 4),  (size_t)4);
    CHECK_EQ(SafeCut(s, 11), (size_t)10);  // середина последней
    CHECK_EQ(SafeCut(s, 12), (size_t)12);
}

// ============================================================
// Integration: общий хелпер
// ============================================================

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("cs_test_" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    void Write(const std::string& name, const std::string& content) const {
        std::ofstream out(path / name, std::ios::binary);
        out << content;
    }
    void WritePath(const std::string& rel, const std::string& content) const {
        fs::path p = path / rel;
        if (p.has_parent_path() && p.parent_path() != path) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        std::ofstream out(p, std::ios::binary);
        out << content;
    }
};

static file_searcher::SearchOptions MakeOptions(const fs::path& dir) {
    file_searcher::SearchOptions o;
    o.Directory = dir;
    o.Extensions = {".cs"};
    o.ExcludeDirs = {};
    o.ExcludeFiles = {};
    o.MaxFileSizeMb = -1;
    o.ShowFirstN = 1000;
    return o;
}

// ============================================================
// Integration: Word с границами + MatchCount
// ============================================================

static void Test_Int_Word_Bounded() {
    Section("Int: Word (bounded)");
    TempDir t; t.Write("test.cs", "name username name\nfoo name\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Word;
    req.Word = "name";
    req.WordWholeOnly = true;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[0].MatchCount, 2);
        CHECK_EQ(res[1].Line, 2);
        CHECK_EQ(res[1].MatchCount, 1);
    }
}

// ============================================================
// Integration: Word без границ
// ============================================================

static void Test_Int_Word_Unbounded() {
    Section("Int: Word (unbounded)");
    TempDir t; t.Write("test.cs", "name username filename\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Word;
    req.Word = "name";
    req.WordWholeOnly = false;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[0].MatchCount, 3);
    }
}

// ============================================================
// Integration: Format — Email
// ============================================================

static void Test_Int_Format_Email() {
    Section("Int: Format Email");
    TempDir t; t.Write("test.cs",
        "e1 = \"admin@company.com\";\n"
        "bad = \"@no-user.com\";\n"
        "e2 = \"help@mail.ru\";\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Format;
    req.Format = patterns::FormatType::Email;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[1].Line, 3);
    }
}

// ============================================================
// Integration: Format — IP
// ============================================================

static void Test_Int_Format_IP() {
    Section("Int: Format IP");
    TempDir t; t.Write("test.cs",
        "ip1 = \"192.168.1.1\";\n"
        "bad = \"192.168.1.1.1\";\n"
        "ip2 = \"10.0.0.1\";\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Format;
    req.Format = patterns::FormatType::Ip;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[1].Line, 3);
    }
}

// ============================================================
// Integration: Quantity Numbers — Equal 3
// ============================================================

static void Test_Int_Numbers_Equal3() {
    Section("Int: Numbers Equal 3");
    TempDir t; t.Write("test.cs",
        "x = 999;\n"
        "y = 42;\n"
        "z = 1000;\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::Numbers;
    req.NumberCompareValue = regex_builder::NumberCompare::Equal;
    req.MinCount = 3;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: Quantity Numbers — Greater 2
// ============================================================

static void Test_Int_Numbers_Greater2() {
    Section("Int: Numbers Greater 2");
    TempDir t; t.Write("test.cs",
        "a = 5;\n"
        "b = 999;\n"
        "c = 12345;\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::Numbers;
    req.NumberCompareValue = regex_builder::NumberCompare::Greater;
    req.MinCount = 2;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
}

// ============================================================
// Integration: LineLength > 10
// ============================================================

static void Test_Int_LineLength() {
    Section("Int: LineLength > 10");
    TempDir t; t.Write("test.cs",
        "short\n"
        "this_is_long_line\n"
        "mid\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::LineLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Greater;
    req.MinCount = 10;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 2);
}

// ============================================================
// Integration: XOrY
// ============================================================

static void Test_Int_XOrY() {
    Section("Int: XOrY");
    TempDir t; t.Write("test.cs", "foo bar\nbaz\nqux\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Combination;
    req.Combination = regex_builder::CombinationType::XOrY;
    req.X = "foo, baz";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[1].Line, 2);
    }
}

// ============================================================
// Integration: Structure XYZ
// ============================================================

static void Test_Int_StructureXYZ() {
    Section("Int: Structure X->Y->Z");
    TempDir t; t.Write("test.cs",
        "a b c\n"
        "a x y z\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XAnywhereBeforeYBeforeZ;
    req.X = "a"; req.Y = "b"; req.Z = "c";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: XButNotY по позиции
// ============================================================

static void Test_Int_XButNotY_ByPos() {
    Section("Int: X но не Y (by pos)");
    TempDir t; t.Write("test.cs",
        "catch (Exception) { }\n"
        "catch { }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Combination;
    req.Combination = regex_builder::CombinationType::XButNotY;
    req.XButNotYModeValue = regex_builder::XButNotYMode::ByPosition;
    req.X = "catch";
    req.Y = "catch (Exception)";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 2);
}

// ============================================================
// Integration: XButNotY по строке
// ============================================================

static void Test_Int_XButNotY_ByLine() {
    Section("Int: X но не Y (by line)");
    TempDir t; t.Write("test.cs",
        "if (x) { Console.WriteLine(); }\n"
        "if (y) { }\n"
        "if (z) {\n"
        "    Console.WriteLine();\n"
        "}\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Combination;
    req.Combination = regex_builder::CombinationType::XButNotY;
    req.XButNotYModeValue = regex_builder::XButNotYMode::ByLine;
    req.X = "if";
    req.Y = "Console";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 2);
        CHECK_EQ(res[1].Line, 3);
        CHECK_EQ(res[0].MatchCount, 1);
    }
}

// ============================================================
// Integration: EmptyBlocks (strictly empty)
// ============================================================

static void Test_Int_EmptyBlocks() {
    Section("Int: EmptyBlocks");
    TempDir t; t.Write("test.cs",
        "if (true) { }\n"
        "if (false) { x; }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "if";
    req.EmptyMetricValue = regex_builder::EmptyMetric::StrictlyEmpty;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

static void Test_Int_EmptyBlocks_XInComment() {
    Section("Int: EmptyBlocks (regression: X in comment)");
    TempDir t; t.Write("test.cs",
        "if (x) // if (y)\n"
        "{ }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "if";
    req.EmptyMetricValue = regex_builder::EmptyMetric::StrictlyEmpty;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: Comments — StartLine
// ============================================================

static void Test_Int_Comments_StartLine() {
    Section("Int: Comments (StartLine)");
    TempDir t; t.Write("test.cs",
        "// TODO: fix\n"
        "int x = 5; // note\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: Comments — EndLine
// ============================================================

static void Test_Int_Comments_EndLine() {
    Section("Int: Comments (EndLine)");
    TempDir t; t.Write("test.cs",
        "// TODO: fix\n"
        "int x = 5; // note\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::EndLine;
    req.Language = "C#";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[1].Line, 2);
    }
}

// ============================================================
// Integration: ArgsLength (basic)
// ============================================================

static void Test_Int_ArgsLength() {
    Section("Int: ArgsLength");
    TempDir t; t.Write("test.cs",
        "void f(int x, int y) { }\n"
        "g(1, 2);\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Equal;
    req.MinCount = 2;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Line, 1);
        CHECK_EQ(res[0].Kind, std::string("decl"));
        CHECK_EQ(res[1].Line, 2);
        CHECK_EQ(res[1].Kind, std::string("call"));
    }
}

// ============================================================
// Integration: EmptyBlocks — else / else if
// ============================================================

static void Test_Int_EmptyBlocks_ElseIf() {
    Section("Int: EmptyBlocks (else / else if)");
    TempDir t; t.Write("test.cs",
        "if (x) { }\n"
        "else if (y) { }\n"
        "else { }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "else";
    req.EmptyMetricValue = regex_builder::EmptyMetric::StrictlyEmpty;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // `else if (y) { }` отсекается специальной проверкой; `else { }` находится.
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 3);
}

// ============================================================
// Integration: EmptyBlocks — metric MaxChars
// ============================================================

static void Test_Int_EmptyBlocks_MaxChars() {
    Section("Int: EmptyBlocks (MaxChars 3)");
    TempDir t; t.Write("test.cs",
        "if (a) { }\n"
        "if (b) { x; }\n"
        "if (c) { xy; }\n"
        "if (d) { xyz; }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "if";
    req.EmptyMetricValue = regex_builder::EmptyMetric::MaxChars;
    req.EmptyMaxCount = 3;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // a: 0 non-ws, b: 2, c: 3, d: 4 → d не проходит
    CHECK_EQ((int)res.size(), 3);
}

// ============================================================
// Integration: EmptyBlocks — metric MaxLines
// ============================================================

static void Test_Int_EmptyBlocks_MaxLines() {
    Section("Int: EmptyBlocks (MaxLines 0)");
    TempDir t; t.Write("test.cs",
        "if (a) { }\n"
        "if (b) {\n"
        "}\n"
        "if (c) {\n"
        "\n"
        "}\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "if";
    req.EmptyMetricValue = regex_builder::EmptyMetric::MaxLines;
    req.EmptyMaxCount = 0;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // a: 0 \n, b: 1 \n, c: 2 \n → только a
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: EmptyBlocks — catch
// ============================================================

static void Test_Int_EmptyBlocks_Catch() {
    Section("Int: EmptyBlocks (catch)");
    TempDir t; t.Write("test.cs",
        "catch { }\n"
        "catch (Exception) { }\n"
        "catch (Exception ex) { throw; }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XEmpty;
    req.X = "catch";
    req.EmptyMetricValue = regex_builder::EmptyMetric::StrictlyEmpty;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // Первые два пустые, третий содержит throw
    CHECK_EQ((int)res.size(), 2);
}

// ============================================================
// Integration: ArgsLength — шаблонные вызовы
// ============================================================

static void Test_Int_ArgsLength_Template() {
    Section("Int: ArgsLength (template)");
    TempDir t; t.Write("test.cs",
        "Foo<int>(1, 2);\n"
        "Foo<Bar<int>>(a, b, c);\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Greater;
    req.MinCount = 0;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    if (res.size() == 2) {
        CHECK_EQ(res[0].Kind, std::string("call"));
        CHECK_EQ(res[1].Kind, std::string("call"));
    }
}

// ============================================================
// Integration: ArgsLength — p->foo
// ============================================================

static void Test_Int_ArgsLength_Arrow() {
    Section("Int: ArgsLength (->)");
    TempDir t; t.Write("test.cs", "p->foo(1, 2);\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Equal;
    req.MinCount = 2;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Kind, std::string("call"));
}

// ============================================================
// Integration: ArgsLength — 0 аргументов
// ============================================================

static void Test_Int_ArgsLength_Zero() {
    Section("Int: ArgsLength (0)");
    TempDir t; t.Write("test.cs",
        "foo();\n"
        "bar(1);\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Equal;
    req.MinCount = 0;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: ArgsLength — вложенность
// ============================================================

static void Test_Int_ArgsLength_Nested() {
    Section("Int: ArgsLength (nested)");
    TempDir t; t.Write("test.cs", "foo(bar(baz(1)));\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Equal;
    req.MinCount = 1;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // Все три (foo/bar/baz) имеют по 1 аргументу
    CHECK_EQ((int)res.size(), 3);
}

// ============================================================
// Integration: ArgsLength — keyword skip
// ============================================================

static void Test_Int_ArgsLength_Keyword() {
    Section("Int: ArgsLength (keyword skip)");
    TempDir t; t.Write("test.cs",
        "if (x) { }\n"
        "while (y) { }\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Quantity;
    req.Quantity = regex_builder::QuantityType::ArgsLength;
    req.NumberCompareValue = regex_builder::NumberCompare::Greater;
    req.MinCount = 0;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 0);
}

// ============================================================
// Integration: Comments — блочный
// ============================================================

static void Test_Int_Comments_Block() {
    Section("Int: Comments (block)");
    TempDir t; t.Write("test.cs",
        "/* TODO: fix */\n"
        "int x = 5;\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: Comments — // внутри строки
// ============================================================

static void Test_Int_Comments_InString() {
    Section("Int: Comments (in string)");
    TempDir t; t.Write("test.cs",
        "string s = \"// TODO: not a comment\";\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 0);
}

// ============================================================
// Integration: Comments — регистрозависимость
// ============================================================

static void Test_Int_Comments_CaseSensitive() {
    Section("Int: Comments (case)");
    TempDir t; t.Write("test.cs",
        "// todo: fix\n"
        "// TODO: fix\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";
    req.CommentWord = "TODO";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // "// todo" не находит (регистрозависимо)
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 2);
}

// ============================================================
// Integration: Comments — слово внутри
// ============================================================

static void Test_Int_Comments_Word() {
    Section("Int: Comments (word)");
    TempDir t; t.Write("test.cs",
        "// hello world\n"
        "// goodbye world\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";
    req.CommentWord = "hello";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: StructureXYZ — многострочный
// ============================================================

static void Test_Int_StructureXYZ_MultiLine() {
    Section("Int: StructureXYZ (multiline)");
    TempDir t; t.Write("test.cs", "a\nb\nc\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XAnywhereBeforeYBeforeZ;
    req.X = "a"; req.Y = "b"; req.Z = "c";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    // StructureXYZ показывает строку Y → строка 2
    if (!res.empty()) CHECK_EQ(res[0].Line, 2);
}

// ============================================================
// Integration: StructureXYZ — нет Z
// ============================================================

static void Test_Int_StructureXYZ_NoMatch() {
    Section("Int: StructureXYZ (no Z)");
    TempDir t; t.Write("test.cs", "a b\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Structure;
    req.Structure = regex_builder::StructureType::XAnywhereBeforeYBeforeZ;
    req.X = "a"; req.Y = "b"; req.Z = "c";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 0);
}

// ============================================================
// Integration: XButNotY по позиции — без пересечения
// ============================================================

static void Test_Int_XButNotY_ByPos_NoOverlap() {
    Section("Int: X но не Y (no overlap)");
    TempDir t; t.Write("test.cs", "foo(bar);\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Combination;
    req.Combination = regex_builder::CombinationType::XButNotY;
    req.XButNotYModeValue = regex_builder::XButNotYMode::ByPosition;
    req.X = "foo";
    req.Y = "bar";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // foo и bar рядом, но не пересекаются → foo находится
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) CHECK_EQ(res[0].Line, 1);
}

// ============================================================
// Integration: MatchCount в XOrY
// ============================================================

static void Test_Int_MatchCount_XOrY() {
    Section("Int: MatchCount (XOrY)");
    TempDir t; t.Write("test.cs",
        "foo bar\n"
        "foo baz\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Combination;
    req.Combination = regex_builder::CombinationType::XOrY;
    req.X = "foo, bar";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    // foo, bar, foo — три отдельных результата, каждый MatchCount=1
    CHECK_EQ((int)res.size(), 3);
    for (const auto& r : res) CHECK_EQ(r.MatchCount, 1);
}

// ============================================================
// Integration: MatchCount в Comments
// ============================================================

static void Test_Int_MatchCount_Comments() {
    Section("Int: MatchCount (Comments)");
    TempDir t; t.Write("test.cs",
        "// a\n"
        "// b\n");
    auto opts = MakeOptions(t.path);

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Comment;
    req.Comment = regex_builder::CommentType::StartLine;
    req.Language = "C#";

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 2);
    for (const auto& r : res) CHECK_EQ(r.MatchCount, 1);
}


// ============================================================
// Integration: ExcludeFiles маска
// ============================================================

static void Test_Int_ExcludeFiles() {
    Section("Int: ExcludeFiles mask");
    TempDir t;
    t.Write("a.cs",     "foo\n");
    t.Write("b.g.cs",   "foo\n");
    auto opts = MakeOptions(t.path);
    opts.ExcludeFiles = {"*.g.cs"};

    regex_builder::SearchRequest req;
    req.Type = regex_builder::SearchType::Word;
    req.Word = "foo";
    req.WordWholeOnly = false;

    auto res = file_searcher::Search(req, regex_builder::Build(req), opts);
    CHECK_EQ((int)res.size(), 1);
    if (!res.empty()) {
        std::string rel = utf8::ToUtf8(fs::relative(res[0].File, t.path));
        CHECK_EQ(rel, std::string("a.cs"));
    }
}

// ============================================================
// main
// ============================================================

int main() {
    std::srand((unsigned)std::time(nullptr));
    settings::SetDefaults();

    Test_Escape();
    Test_EscapeXyz();
    Test_EscapeXyzWithBounds();
    Test_BuildWord();
    Test_BuildCombination_XOrY();
    Test_BuildStructure_XYZ();
    Test_BuildEmpty();
    Test_BuildComment();
    Test_BuildQuantity_Numbers();
    Test_BuildQuantity_LineLength();
    Test_ForFormat();
    Test_SafeCut();

    Test_Int_Word_Bounded();
    Test_Int_Word_Unbounded();
    Test_Int_Format_Email();
    Test_Int_Format_IP();
    Test_Int_Numbers_Equal3();
    Test_Int_Numbers_Greater2();
    Test_Int_LineLength();
    Test_Int_XOrY();
    Test_Int_StructureXYZ();
    Test_Int_XButNotY_ByPos();
    Test_Int_XButNotY_ByLine();
        Test_Int_EmptyBlocks();
    Test_Int_EmptyBlocks_XInComment();
    Test_Int_EmptyBlocks_ElseIf();
    Test_Int_EmptyBlocks_MaxChars();
    Test_Int_EmptyBlocks_MaxLines();
    Test_Int_EmptyBlocks_Catch();
    Test_Int_Comments_StartLine();
    Test_Int_Comments_EndLine();
    Test_Int_Comments_Block();
    Test_Int_Comments_InString();
    Test_Int_Comments_CaseSensitive();
    Test_Int_Comments_Word();
    Test_Int_ArgsLength();
    Test_Int_ArgsLength_Template();
    Test_Int_ArgsLength_Arrow();
    Test_Int_ArgsLength_Zero();
    Test_Int_ArgsLength_Nested();
    Test_Int_ArgsLength_Keyword();
    Test_Int_StructureXYZ_MultiLine();
    Test_Int_StructureXYZ_NoMatch();
    Test_Int_XButNotY_ByPos_NoOverlap();
    Test_Int_MatchCount_XOrY();
    Test_Int_MatchCount_Comments();
    Test_Int_ExcludeFiles();

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << g_passed << ", Failed: " << g_failed << "\n";
    std::cout << "========================================\n";
    return g_failed == 0 ? 0 : 1;
}