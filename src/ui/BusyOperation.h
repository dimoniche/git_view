#pragma once

#include <QApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QString>
#include <QWidget>
#include <QtConcurrent>

#include <type_traits>
#include <utility>

namespace BusyOperation {

// Runs work on a worker thread while showing an indeterminate modal progress dialog.
template <typename Func>
auto run(QWidget *parent, const QString &label, Func &&func) -> std::invoke_result_t<Func>
{
    using Result = std::invoke_result_t<Func>;

    QProgressDialog dialog(label, QString(), 0, 0, parent);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setMinimumDuration(0);
    dialog.setCancelButton(nullptr);
    dialog.setMinimumWidth(320);
    dialog.setValue(0);
    dialog.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QFutureWatcher<Result> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run(std::forward<Func>(func)));
    loop.exec();

    dialog.reset();
    return watcher.result();
}

} // namespace BusyOperation
