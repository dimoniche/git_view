#pragma once

#include <QString>

class QTextCharFormat;

enum class CodeLanguage {
    Generic,
    Cpp,
    Python,
    Shell,
    Json,
};

enum class CodeBlockState {
    Normal = 0,
    StringDouble,
    StringSingle,
    BlockComment,
    PythonTripleDouble,
    PythonTripleSingle,
};

CodeLanguage codeLanguageForPath(const QString &path);

void highlightCodeSyntax(CodeLanguage language, const QString &text, int previousBlockState,
                         int *nextBlockState,
                         const QTextCharFormat &keywordFormat, const QTextCharFormat &stringFormat,
                         const QTextCharFormat &commentFormat, const QTextCharFormat &numberFormat,
                         const QTextCharFormat &preprocessorFormat,
                         void (*applyFormat)(int start, int length, const QTextCharFormat &format,
                                             void *userData),
                         void *userData);
