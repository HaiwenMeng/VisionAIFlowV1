#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionAIFlowCli"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption doctorOption(QStringLiteral("doctor"), QStringLiteral("Verify frozen local toolchain and dependency root paths"));
    parser.addOption(doctorOption);
    parser.process(application);
    if (!parser.isSet(doctorOption))
    {
        QTextStream(stderr) << "Specify --doctor or --help.\n";
        return 2;
    }

    const QJsonObject expectedPaths{
        {QStringLiteral("cmake"), QStringLiteral("F:/Qt6.9.2/Tools/CMake_64/bin/cmake.exe")},
        {QStringLiteral("qt"), QStringLiteral("F:/Qt6.9.2/6.9.2/msvc2022_64/bin/qmake.exe")},
        {QStringLiteral("msvc143_14_36"), QStringLiteral("F:/VS2022/BuildTools/VC/Tools/MSVC/14.36")},
        {QStringLiteral("cuda118"), QStringLiteral("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin/nvcc.exe")},
        {QStringLiteral("dependencies"), QStringLiteral("F:/VisionAIFlowDeps")}
    };
    QJsonArray checks;
    bool healthy = true;
    for (auto it = expectedPaths.begin(); it != expectedPaths.end(); ++it)
    {
        const bool present = QFile::exists(it.value().toString());
        checks.append(QJsonObject{{QStringLiteral("name"), it.key()}, {QStringLiteral("path"), it.value().toString()}, {QStringLiteral("present"), present}});
        healthy = healthy && present;
    }
    const QJsonObject report{{QStringLiteral("product"), QStringLiteral("VisionAIFlowCli")}, {QStringLiteral("healthy"), healthy}, {QStringLiteral("checks"), checks}};
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Compact) << '\n';
    return healthy ? 0 : 1;
}
