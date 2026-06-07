#include "ui/DiffViewerDialog.h"

#include "ui/DiffViewerWidget.h"

#include <QVBoxLayout>

DiffViewerDialog::DiffViewerDialog(const QString &title, const QString &diff, QWidget *parent,
                                   const DiffViewerSources &sources)
    : QDialog(parent)
{
    setWindowTitle(title);
    setWindowFlag(Qt::Window);
    resize(1200, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *viewer = new DiffViewerWidget(this);
    viewer->setDiff(diff);
    if (!sources.before.isEmpty() || !sources.after.isEmpty()) {
        viewer->setSources(sources.before, sources.after, sources.beforeCaption,
                             sources.afterCaption);
    }

    layout->addWidget(viewer);
}

void DiffViewerDialog::showDiff(QWidget *parent, const QString &title, const QString &text,
                                const DiffViewerSources &sources)
{
    auto *dialog = new DiffViewerDialog(title, text, parent, sources);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DiffViewerDialog::showSource(QWidget *parent, const QString &title, const QString &text)
{
    auto *dialog = new DiffViewerDialog(title, text, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
