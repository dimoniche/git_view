#include "ui/DiffViewerDialog.h"

#include "ui/DialogTitleBar.h"
#include "ui/DiffViewerWidget.h"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QSizeGrip>
#include <QVBoxLayout>

DiffViewerDialog::DiffViewerDialog(const QString &title, const QString &diff, QWidget *parent,
                                   const DiffViewerSources &sources,
                                   const QString &sourceFilePath)
    : QWidget(parent)
{
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(480, 320);
    resize(1200, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(new DialogTitleBar(title, this));

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto *viewer = new DiffViewerWidget(content);
    viewer->setDiff(diff);
    if (!sourceFilePath.isEmpty()) {
        viewer->setSourceFilePath(sourceFilePath);
    }
    if (!sources.before.isEmpty() || !sources.after.isEmpty()) {
        viewer->setSources(sources.before, sources.after, sources.beforeCaption,
                           sources.afterCaption);
    }

    contentLayout->addWidget(viewer, 1);

    auto *gripRow = new QHBoxLayout();
    gripRow->addStretch();
    gripRow->addWidget(new QSizeGrip(content));
    contentLayout->addLayout(gripRow);

    layout->addWidget(content, 1);

    auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);
}

void DiffViewerDialog::showDiff(QWidget *parent, const QString &title, const QString &text,
                                const DiffViewerSources &sources,
                                const QString &sourceFilePath)
{
    auto *dialog = new DiffViewerDialog(title, text, parent, sources, sourceFilePath);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DiffViewerDialog::showSource(QWidget *parent, const QString &title, const QString &text)
{
    auto *dialog = new DiffViewerDialog(title, text, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
