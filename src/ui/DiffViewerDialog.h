#pragma once

#include <QWidget>
#include <QString>

struct DiffViewerSources {
    QString before;
    QString after;
    QString beforeCaption;
    QString afterCaption;
};

class DiffViewerDialog : public QWidget {
    Q_OBJECT

public:
    explicit DiffViewerDialog(const QString &title, const QString &diff, QWidget *parent = nullptr,
                             const DiffViewerSources &sources = {});

    static void showDiff(QWidget *parent, const QString &title, const QString &diff,
                         const DiffViewerSources &sources = {});
    static void showSource(QWidget *parent, const QString &title, const QString &text);
};
