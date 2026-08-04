#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QCoreApplication>
#include <QCborMap>

#include <functional>

namespace visionaiflow::qt_foundation
{
using HostOperationHandler = std::function<foundation::Result<QCborMap>(const QCborMap &request)>;
using HostAsyncResponder = std::function<foundation::Result<void>(const QString &type, const QCborMap &payload)>;
using HostAsyncOperationHandler = std::function<foundation::Result<void>(const QCborMap &request, const HostAsyncResponder &responder)>;

int RunHostApplication(QCoreApplication &application, const QString &hostRole, const QString &runtimeVersion, HostOperationHandler operationHandler = {}, HostAsyncOperationHandler asyncOperationHandler = {});
}
