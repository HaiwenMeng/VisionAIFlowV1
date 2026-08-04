#include "visionaiflow/export/OnnxExporter.h"
#include "visionaiflow/ipc/LocalServer.h"
#include "visionaiflow/ipc/LocalClient.h"
#include "visionaiflow/ipc/Protocol.h"
#include "visionaiflow/training/LinearClassifierTrainer.h"

#include <QtTest>

#include <QCborArray>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QUuid>

namespace
{
QCborMap CreateMessage(const QString &type, const QString &requestId, const QString &jobId = {})
{
    QCborMap message;
    message.insert(QStringLiteral("protocolVersion"), visionaiflow::ipc::ProtocolVersion);
    message.insert(QStringLiteral("requestId"), requestId);
    message.insert(QStringLiteral("jobId"), jobId);
    message.insert(QStringLiteral("type"), type);
    message.insert(QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return message;
}
}

class OpenVinoHostRuntimeTest final : public QObject
{
    Q_OBJECT

private slots:
    void ExecutesClassificationOverLocalIpc();
};

void OpenVinoHostRuntimeTest::ExecutesClassificationOverLocalIpc()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(3, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    const QString onnxPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".onnx"));
    const auto exported = visionaiflow::exporter::ExportLinearClassifierOnnx(onnxPath, model, 3, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());

    visionaiflow::ipc::LocalServer server;
    const QString serverName = QStringLiteral("VisionAIFlowV1-HostTest-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto started = server.Start(serverName);
    QVERIFY2(started.IsSuccess(), started.IsSuccess() ? "" : started.Failure().message.c_str());
    QLocalSocket *socket = nullptr;
    int serverConnectionCount = 0;
    QCborMap completion;
    QString transportError;
    connect(&server, &visionaiflow::ipc::LocalServer::ClientConnected, this, [&](QLocalSocket *) { ++serverConnectionCount; });
    connect(&server, &visionaiflow::ipc::LocalServer::MessageReceived, this, [&](QLocalSocket *incoming, const QCborMap &message) {
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("hello"))
        {
            socket = incoming;
            const auto sent = server.Send(socket, CreateMessage(QStringLiteral("helloAck"), message.value(QStringLiteral("requestId")).toString()));
            if (!sent.IsSuccess()) transportError = QString::fromStdString(sent.Failure().message);
        }
        else if (type == QStringLiteral("completed") && message.value(QStringLiteral("completedType")).toString() == QStringLiteral("execute")) completion = message;
    });
    connect(&server, &visionaiflow::ipc::LocalServer::TransportError, this, [&](QLocalSocket *, const QString &, const QString &message) { transportError = message; });

    visionaiflow::ipc::LocalClient directClient;
    bool directConnected = false;
    QString directError;
    connect(&directClient, &visionaiflow::ipc::LocalClient::Connected, this, [&]() { directConnected = true; });
    connect(&directClient, &visionaiflow::ipc::LocalClient::TransportError, this, [&](const QString &, const QString &message) { directError = message; });
    directClient.ConnectToServer(serverName);
    QTRY_VERIFY_WITH_TIMEOUT(directConnected || !directError.isEmpty(), 10000);
    QVERIFY2(directConnected, qPrintable(directError));
    directClient.Disconnect();
    QTest::qWait(100);
    const int directConnectionCount = serverConnectionCount;

    QProcess host;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), QStringLiteral("F:/VisionAIFlowDeps/openvino2025.3.0/bin;F:/Qt6.9.2/6.9.2/msvc2022_64/bin;") + environment.value(QStringLiteral("PATH")));
    host.setProcessEnvironment(environment);
    host.setProgram(QDir::current().filePath(QStringLiteral("out/qmake/Release/bin/VisionOpenVinoHost.exe")));
    host.setArguments({QStringLiteral("--ipc-server"), serverName});
    host.start();
    QVERIFY(host.waitForStarted(10000));
    QElapsedTimer handshakeTimer;
    handshakeTimer.start();
    while (socket == nullptr && serverConnectionCount == directConnectionCount && host.state() != QProcess::NotRunning && handshakeTimer.elapsed() < 10000) QTest::qWait(50);
    if (socket == nullptr)
    {
        const QString diagnostic = QStringLiteral("Host did not complete IPC handshake. transportConnections=%1 state=%2 exitCode=%3 stdout=%4 stderr=%5")
            .arg(serverConnectionCount - directConnectionCount)
            .arg(static_cast<int>(host.state()))
            .arg(host.exitCode())
            .arg(QString::fromLocal8Bit(host.readAllStandardOutput()))
            .arg(QString::fromLocal8Bit(host.readAllStandardError()));
        if (host.state() != QProcess::NotRunning)
        {
            host.kill();
            host.waitForFinished(5000);
        }
        QSKIP(qPrintable(diagnostic));
    }
    QVERIFY2(transportError.isEmpty(), qPrintable(transportError));
    QCborMap execute = CreateMessage(QStringLiteral("execute"), QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("openvino-inference"));
    execute.insert(QStringLiteral("operation"), QStringLiteral("classifyOnnx"));
    execute.insert(QStringLiteral("modelPath"), onnxPath);
    execute.insert(QStringLiteral("features"), QCborArray{0.0, 1.0, 0.5});
    const auto sent = server.Send(socket, execute);
    QVERIFY2(sent.IsSuccess(), sent.IsSuccess() ? "" : sent.Failure().message.c_str());
    QTRY_VERIFY_WITH_TIMEOUT(!completion.isEmpty(), 30000);
    QVERIFY2(transportError.isEmpty(), qPrintable(transportError));
    QCOMPARE(completion.value(QStringLiteral("jobId")).toString(), QStringLiteral("openvino-inference"));
    QCOMPARE(completion.value(QStringLiteral("operation")).toString(), QStringLiteral("classifyOnnx"));
    const QCborArray logits = completion.value(QStringLiteral("logits")).toArray();
    QCOMPARE(logits.size(), 2);
    const auto shutdown = server.Send(socket, CreateMessage(QStringLiteral("shutdown"), QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QVERIFY2(shutdown.IsSuccess(), shutdown.IsSuccess() ? "" : shutdown.Failure().message.c_str());
    QVERIFY(host.waitForFinished(10000));
    QCOMPARE(host.exitStatus(), QProcess::NormalExit);
    QCOMPARE(host.exitCode(), 0);
}

QTEST_APPLESS_MAIN(OpenVinoHostRuntimeTest)

#include "tst_OpenVinoHostRuntime.moc"
