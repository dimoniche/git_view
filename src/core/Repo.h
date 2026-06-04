#pragma once

#include <QString>

class Repo {
public:
    explicit Repo(QString path = {});

    bool isValid() const { return !m_path.isEmpty(); }
    const QString &path() const { return m_path; }
    void setPath(QString path) { m_path = std::move(path); }

private:
    QString m_path;
};
