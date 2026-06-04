#include "core/Repo.h"

Repo::Repo(QString path)
    : m_path(std::move(path))
{
}
