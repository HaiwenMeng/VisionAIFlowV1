#pragma once

#include <QMainWindow>

#include "visionaiflow/app/HostSupervisor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

namespace visionaiflow::app
{
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void StartHost(const QString &role, const QString &executableName);
    void SetStatus(const QString &role, const QString &state);

    Ui::MainWindow *ui;
    HostSupervisor m_supervisor;
};
}
