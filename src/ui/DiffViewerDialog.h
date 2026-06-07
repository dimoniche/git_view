#pragma once

#include <QDialog>

class DiffViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit DiffViewerDialog(const QString &title, const QString &text, QWidget *parent = nullptr);

    static void showDiff(QWidget *parent, const QString &title, const QString &text);
};
