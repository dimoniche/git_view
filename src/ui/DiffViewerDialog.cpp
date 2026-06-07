#include "ui/DiffViewerDialog.h"

#include "ui/DiffHighlighter.h"

#include <QFont>
#include <QFontMetrics>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {

QFont diffMonospaceFont(const QFont &base)
{
    QFont font = base;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    font.setFamily(QStringLiteral("Monospace"));
#endif
    return font;
}

} // namespace

DiffViewerDialog::DiffViewerDialog(const QString &title, const QString &text, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setWindowFlag(Qt::Window);
    resize(900, 640);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *view = new QPlainTextEdit(this);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFont(diffMonospaceFont(view->font()));
    view->setTabStopDistance(
        QFontMetrics(view->font()).horizontalAdvance(QLatin1Char(' ')) * 4);
    view->setPlainText(text);
    new DiffHighlighter(view->document());

    layout->addWidget(view);
}

void DiffViewerDialog::showDiff(QWidget *parent, const QString &title, const QString &text)
{
    auto *dialog = new DiffViewerDialog(title, text, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
