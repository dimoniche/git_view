#include "ui/CommitDetailsPanel.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

CommitDetailsPanel::CommitDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Commit"), this));

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summaryLabel);

    layout->addWidget(new QLabel(tr("Changed files"), this));

    m_filesList = new QListWidget(this);
    layout->addWidget(m_filesList, 1);

    clear();
}

void CommitDetailsPanel::showDetails(const CommitDetails &details)
{
    m_summaryLabel->setText(
        tr("<b>%1</b><br/>%2<br/>%3<br/><code>%4</code>")
            .arg(details.commit.subject.toHtmlEscaped(),
                 details.commit.author.toHtmlEscaped(),
                 details.commit.date.toHtmlEscaped(),
                 details.commit.hash.toHtmlEscaped()));

    m_filesList->clear();
    for (const CommitFileChange &change : details.files) {
        m_filesList->addItem(QStringLiteral("[%1] %2").arg(change.status, change.path));
    }

    if (details.files.empty()) {
        m_filesList->addItem(tr("(no file changes)"));
    }
}

void CommitDetailsPanel::clear()
{
    m_summaryLabel->setText(tr("Select a commit"));
    m_filesList->clear();
}
