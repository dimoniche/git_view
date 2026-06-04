#pragma once

#include "core/CommitDetails.h"

#include <QWidget>

class QLabel;
class QListWidget;

class CommitDetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit CommitDetailsPanel(QWidget *parent = nullptr);

    void showDetails(const CommitDetails &details);
    void clear();

private:
    QLabel *m_summaryLabel = nullptr;
    QListWidget *m_filesList = nullptr;
};
