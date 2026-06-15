#include "ui/CodeSyntax.h"

#include <QChar>
#include <QHash>
#include <QSet>
#include <QTextCharFormat>

namespace {

bool isIdentifierStart(QChar ch)
{
    return ch.isLetter() || ch == QLatin1Char('_');
}

bool isIdentifierPart(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

const QSet<QString> &cppKeywords()
{
    static const QSet<QString> words = {
        QStringLiteral("alignas"),   QStringLiteral("alignof"),  QStringLiteral("and"),
        QStringLiteral("and_eq"),    QStringLiteral("asm"),      QStringLiteral("auto"),
        QStringLiteral("bitand"),    QStringLiteral("bitor"),    QStringLiteral("bool"),
        QStringLiteral("break"),     QStringLiteral("case"),     QStringLiteral("catch"),
        QStringLiteral("char"),      QStringLiteral("class"),    QStringLiteral("compl"),
        QStringLiteral("const"),     QStringLiteral("consteval"),QStringLiteral("constexpr"),
        QStringLiteral("constinit"), QStringLiteral("const_cast"),QStringLiteral("continue"),
        QStringLiteral("co_await"),  QStringLiteral("co_return"),QStringLiteral("co_yield"),
        QStringLiteral("decltype"),  QStringLiteral("default"),  QStringLiteral("delete"),
        QStringLiteral("do"),        QStringLiteral("double"),   QStringLiteral("dynamic_cast"),
        QStringLiteral("else"),      QStringLiteral("enum"),     QStringLiteral("explicit"),
        QStringLiteral("export"),    QStringLiteral("extern"),   QStringLiteral("false"),
        QStringLiteral("float"),     QStringLiteral("for"),      QStringLiteral("friend"),
        QStringLiteral("goto"),      QStringLiteral("if"),       QStringLiteral("inline"),
        QStringLiteral("int"),       QStringLiteral("long"),     QStringLiteral("mutable"),
        QStringLiteral("namespace"), QStringLiteral("new"),      QStringLiteral("noexcept"),
        QStringLiteral("not"),       QStringLiteral("not_eq"),   QStringLiteral("nullptr"),
        QStringLiteral("operator"),  QStringLiteral("or"),       QStringLiteral("or_eq"),
        QStringLiteral("private"),   QStringLiteral("protected"),QStringLiteral("public"),
        QStringLiteral("register"),  QStringLiteral("reinterpret_cast"),QStringLiteral("requires"),
        QStringLiteral("return"),    QStringLiteral("short"),    QStringLiteral("signed"),
        QStringLiteral("sizeof"),    QStringLiteral("static"),   QStringLiteral("static_assert"),
        QStringLiteral("static_cast"),QStringLiteral("struct"),  QStringLiteral("switch"),
        QStringLiteral("template"),  QStringLiteral("this"),     QStringLiteral("thread_local"),
        QStringLiteral("throw"),     QStringLiteral("true"),     QStringLiteral("try"),
        QStringLiteral("typedef"),   QStringLiteral("typeid"),   QStringLiteral("typename"),
        QStringLiteral("union"),     QStringLiteral("unsigned"), QStringLiteral("using"),
        QStringLiteral("virtual"),   QStringLiteral("void"),     QStringLiteral("volatile"),
        QStringLiteral("wchar_t"),   QStringLiteral("while"),    QStringLiteral("xor"),
        QStringLiteral("xor_eq"),
        // Common in other C-like languages shown in diffs
        QStringLiteral("def"),       QStringLiteral("elif"),     QStringLiteral("except"),
        QStringLiteral("finally"),   QStringLiteral("from"),     QStringLiteral("function"),
        QStringLiteral("import"),    QStringLiteral("in"),       QStringLiteral("let"),
        QStringLiteral("match"),     QStringLiteral("mut"),      QStringLiteral("pass"),
        QStringLiteral("pub"),       QStringLiteral("raise"),    QStringLiteral("self"),
        QStringLiteral("super"),     QStringLiteral("trait"),    QStringLiteral("type"),
        QStringLiteral("uint"),      QStringLiteral("var"),      QStringLiteral("yield"),
        QStringLiteral("async"),     QStringLiteral("await"),    QStringLiteral("impl"),
        QStringLiteral("fn"),        QStringLiteral("package"),  QStringLiteral("interface"),
        QStringLiteral("extends"),   QStringLiteral("final"),    QStringLiteral("override"),
    };
    return words;
}

const QSet<QString> &pythonKeywords()
{
    static const QSet<QString> words = {
        QStringLiteral("and"),       QStringLiteral("as"),       QStringLiteral("assert"),
        QStringLiteral("async"),     QStringLiteral("await"),    QStringLiteral("break"),
        QStringLiteral("class"),     QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"),       QStringLiteral("elif"),     QStringLiteral("else"),
        QStringLiteral("except"),    QStringLiteral("False"),    QStringLiteral("finally"),
        QStringLiteral("for"),       QStringLiteral("from"),     QStringLiteral("global"),
        QStringLiteral("if"),        QStringLiteral("import"),   QStringLiteral("in"),
        QStringLiteral("is"),        QStringLiteral("lambda"),   QStringLiteral("None"),
        QStringLiteral("nonlocal"),  QStringLiteral("not"),      QStringLiteral("or"),
        QStringLiteral("pass"),      QStringLiteral("raise"),    QStringLiteral("return"),
        QStringLiteral("True"),      QStringLiteral("try"),      QStringLiteral("while"),
        QStringLiteral("with"),      QStringLiteral("yield"),
    };
    return words;
}

const QSet<QString> &shellKeywords()
{
    static const QSet<QString> words = {
        QStringLiteral("if"),     QStringLiteral("then"),   QStringLiteral("else"),
        QStringLiteral("elif"),   QStringLiteral("fi"),     QStringLiteral("for"),
        QStringLiteral("do"),     QStringLiteral("done"),   QStringLiteral("case"),
        QStringLiteral("esac"),   QStringLiteral("function"),QStringLiteral("return"),
        QStringLiteral("local"),  QStringLiteral("export"), QStringLiteral("source"),
    };
    return words;
}

void apply(CodeLanguage language, const QString &text, int previousBlockState, int *nextBlockState,
           const QTextCharFormat &keywordFormat, const QTextCharFormat &stringFormat,
           const QTextCharFormat &commentFormat, const QTextCharFormat &numberFormat,
           const QTextCharFormat &preprocessorFormat,
           void (*setFormat)(int, int, const QTextCharFormat &, void *), void *userData)
{
    auto emitFormat = [&](int start, int length, const QTextCharFormat &format) {
        if (length > 0) {
            setFormat(start, length, format, userData);
        }
    };

    const int length = text.length();
    int index = 0;
    int state = previousBlockState;

    auto keywordSet = [&]() -> const QSet<QString> & {
        switch (language) {
        case CodeLanguage::Python:
            return pythonKeywords();
        case CodeLanguage::Shell:
            return shellKeywords();
        case CodeLanguage::Cpp:
        case CodeLanguage::Generic:
        default:
            return cppKeywords();
        }
    };

    while (index < length) {
        const QChar ch = text.at(index);

        if (state == static_cast<int>(CodeBlockState::BlockComment)) {
            int end = text.indexOf(QStringLiteral("*/"), index);
            if (end < 0) {
                emitFormat(index, length - index, commentFormat);
                *nextBlockState = static_cast<int>(CodeBlockState::BlockComment);
                return;
            }
            emitFormat(index, end - index + 2, commentFormat);
            index = end + 2;
            state = static_cast<int>(CodeBlockState::Normal);
            continue;
        }

        if (state == static_cast<int>(CodeBlockState::PythonTripleDouble)
            || state == static_cast<int>(CodeBlockState::PythonTripleSingle)) {
            const QString marker =
                state == static_cast<int>(CodeBlockState::PythonTripleDouble)
                    ? QStringLiteral("\"\"\"")
                    : QStringLiteral("'''");
            int end = text.indexOf(marker, index);
            if (end < 0) {
                emitFormat(index, length - index, stringFormat);
                *nextBlockState = state;
                return;
            }
            emitFormat(index, end - index + 3, stringFormat);
            index = end + 3;
            state = static_cast<int>(CodeBlockState::Normal);
            continue;
        }

        if (state == static_cast<int>(CodeBlockState::StringDouble)
            || state == static_cast<int>(CodeBlockState::StringSingle)) {
            const QChar quote =
                state == static_cast<int>(CodeBlockState::StringDouble) ? QLatin1Char('"')
                                                                         : QLatin1Char('\'');
            int pos = index;
            while (pos < length) {
                if (text.at(pos) == QLatin1Char('\\') && pos + 1 < length) {
                    pos += 2;
                    continue;
                }
                if (text.at(pos) == quote) {
                    emitFormat(index, pos - index + 1, stringFormat);
                    index = pos + 1;
                    state = static_cast<int>(CodeBlockState::Normal);
                    break;
                }
                ++pos;
            }
            if (pos >= length) {
                emitFormat(index, length - index, stringFormat);
                *nextBlockState = state;
                return;
            }
            continue;
        }

        if (language == CodeLanguage::Python && index + 2 < length) {
            if (text.mid(index, 3) == QStringLiteral("\"\"\"")) {
                int end = text.indexOf(QStringLiteral("\"\"\""), index + 3);
                if (end < 0) {
                    emitFormat(index, length - index, stringFormat);
                    *nextBlockState = static_cast<int>(CodeBlockState::PythonTripleDouble);
                    return;
                }
                emitFormat(index, end - index + 3, stringFormat);
                index = end + 3;
                continue;
            }
            if (text.mid(index, 3) == QStringLiteral("'''")) {
                int end = text.indexOf(QStringLiteral("'''"), index + 3);
                if (end < 0) {
                    emitFormat(index, length - index, stringFormat);
                    *nextBlockState = static_cast<int>(CodeBlockState::PythonTripleSingle);
                    return;
                }
                emitFormat(index, end - index + 3, stringFormat);
                index = end + 3;
                continue;
            }
        }

        if ((language == CodeLanguage::Cpp || language == CodeLanguage::Generic)
            && index + 1 < length && text.at(index) == QLatin1Char('/')
            && text.at(index + 1) == QLatin1Char('*')) {
            int end = text.indexOf(QStringLiteral("*/"), index + 2);
            if (end < 0) {
                emitFormat(index, length - index, commentFormat);
                *nextBlockState = static_cast<int>(CodeBlockState::BlockComment);
                return;
            }
            emitFormat(index, end - index + 2, commentFormat);
            index = end + 2;
            continue;
        }

        if (index == 0 && state == static_cast<int>(CodeBlockState::Normal)
            && (language == CodeLanguage::Cpp || language == CodeLanguage::Generic)) {
            int p = 0;
            while (p < length && text.at(p).isSpace()) {
                ++p;
            }
            if (p < length && text.at(p) == QLatin1Char('#')) {
                emitFormat(p, length - p, preprocessorFormat);
                *nextBlockState = static_cast<int>(CodeBlockState::Normal);
                return;
            }
        }

        if (ch == QLatin1Char('#')
            && (language == CodeLanguage::Python || language == CodeLanguage::Shell)) {
            emitFormat(index, length - index, commentFormat);
            *nextBlockState = static_cast<int>(CodeBlockState::Normal);
            return;
        }

        if (ch == QLatin1Char('/') && index + 1 < length && text.at(index + 1) == QLatin1Char('/')
            && (language == CodeLanguage::Cpp || language == CodeLanguage::Generic)) {
            emitFormat(index, length - index, commentFormat);
            *nextBlockState = static_cast<int>(CodeBlockState::Normal);
            return;
        }

        if (ch == QLatin1Char('"')) {
            state = static_cast<int>(CodeBlockState::StringDouble);
            continue;
        }

        if (ch == QLatin1Char('\'')
            && (language != CodeLanguage::Python || (index + 2 < length && text.at(index + 1) != QLatin1Char('\'')))) {
            state = static_cast<int>(CodeBlockState::StringSingle);
            continue;
        }

        if (language == CodeLanguage::Json && ch == QLatin1Char('"')) {
            state = static_cast<int>(CodeBlockState::StringDouble);
            continue;
        }

        if (ch.isDigit()
            && (language == CodeLanguage::Cpp || language == CodeLanguage::Generic
                || language == CodeLanguage::Json || language == CodeLanguage::Python)) {
            int pos = index;
            while (pos < length
                   && (text.at(pos).isDigit() || text.at(pos) == QLatin1Char('.')
                       || text.at(pos) == QLatin1Char('x') || text.at(pos) == QLatin1Char('X')
                       || text.at(pos) == QLatin1Char('a') || text.at(pos) == QLatin1Char('b')
                       || text.at(pos) == QLatin1Char('A') || text.at(pos) == QLatin1Char('B'))) {
                ++pos;
            }
            emitFormat(index, pos - index, numberFormat);
            index = pos;
            continue;
        }

        if (isIdentifierStart(ch)) {
            int pos = index + 1;
            while (pos < length && isIdentifierPart(text.at(pos))) {
                ++pos;
            }
            const QString word = text.mid(index, pos - index);
            if (language == CodeLanguage::Json) {
                if (word == QStringLiteral("true") || word == QStringLiteral("false")
                    || word == QStringLiteral("null")) {
                    emitFormat(index, pos - index, keywordFormat);
                }
            } else if (keywordSet().contains(word)) {
                emitFormat(index, pos - index, keywordFormat);
            }
            index = pos;
            continue;
        }

        ++index;
    }

    *nextBlockState = static_cast<int>(CodeBlockState::Normal);
}

} // namespace

CodeLanguage codeLanguageForPath(const QString &path)
{
    const int dot = path.lastIndexOf(QLatin1Char('.'));
    QString ext;
    if (dot >= 0) {
        ext = path.mid(dot + 1).toLower();
    }

    static const QHash<QString, CodeLanguage> byExtension = {
        {QStringLiteral("c"), CodeLanguage::Cpp},
        {QStringLiteral("h"), CodeLanguage::Cpp},
        {QStringLiteral("cc"), CodeLanguage::Cpp},
        {QStringLiteral("cpp"), CodeLanguage::Cpp},
        {QStringLiteral("cxx"), CodeLanguage::Cpp},
        {QStringLiteral("hpp"), CodeLanguage::Cpp},
        {QStringLiteral("hh"), CodeLanguage::Cpp},
        {QStringLiteral("hxx"), CodeLanguage::Cpp},
        {QStringLiteral("m"), CodeLanguage::Cpp},
        {QStringLiteral("mm"), CodeLanguage::Cpp},
        {QStringLiteral("java"), CodeLanguage::Cpp},
        {QStringLiteral("js"), CodeLanguage::Cpp},
        {QStringLiteral("jsx"), CodeLanguage::Cpp},
        {QStringLiteral("ts"), CodeLanguage::Cpp},
        {QStringLiteral("tsx"), CodeLanguage::Cpp},
        {QStringLiteral("cs"), CodeLanguage::Cpp},
        {QStringLiteral("go"), CodeLanguage::Cpp},
        {QStringLiteral("rs"), CodeLanguage::Cpp},
        {QStringLiteral("kt"), CodeLanguage::Cpp},
        {QStringLiteral("kts"), CodeLanguage::Cpp},
        {QStringLiteral("swift"), CodeLanguage::Cpp},
        {QStringLiteral("php"), CodeLanguage::Cpp},
        {QStringLiteral("cmake"), CodeLanguage::Cpp},
        {QStringLiteral("glsl"), CodeLanguage::Cpp},
        {QStringLiteral("sql"), CodeLanguage::Cpp},
        {QStringLiteral("py"), CodeLanguage::Python},
        {QStringLiteral("pyw"), CodeLanguage::Python},
        {QStringLiteral("pyi"), CodeLanguage::Python},
        {QStringLiteral("sh"), CodeLanguage::Shell},
        {QStringLiteral("bash"), CodeLanguage::Shell},
        {QStringLiteral("zsh"), CodeLanguage::Shell},
        {QStringLiteral("json"), CodeLanguage::Json},
    };

    if (path.endsWith(QStringLiteral("CMakeLists.txt"), Qt::CaseInsensitive)) {
        return CodeLanguage::Cpp;
    }

    const auto it = byExtension.constFind(ext);
    if (it != byExtension.constEnd()) {
        return it.value();
    }

    return CodeLanguage::Generic;
}

void highlightCodeSyntax(CodeLanguage language, const QString &text, int previousBlockState,
                         int *nextBlockState,
                         const QTextCharFormat &keywordFormat, const QTextCharFormat &stringFormat,
                         const QTextCharFormat &commentFormat, const QTextCharFormat &numberFormat,
                         const QTextCharFormat &preprocessorFormat,
                         void (*applyFormat)(int start, int length, const QTextCharFormat &format,
                                             void *userData),
                         void *userData)
{
    apply(language, text, previousBlockState, nextBlockState, keywordFormat, stringFormat,
          commentFormat, numberFormat, preprocessorFormat, applyFormat, userData);
}
