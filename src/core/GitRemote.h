#pragma once

#include <QString>

struct GitRemote {
    QString name;
    QString fetchUrl;
    QString pushUrl;

    bool hasSeparatePushUrl() const
    {
        return !pushUrl.isEmpty() && pushUrl != fetchUrl;
    }
};
