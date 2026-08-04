#include "visionaiflow/domain/JobState.h"

#include <QtTest>

class JobStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void AllowsDefinedLifecycle();
    void RejectsInvalidTransition();
    void IdentifiesTerminalStates();
};

void JobStateTest::AllowsDefinedLifecycle()
{
    QVERIFY(visionaiflow::domain::ValidateTransition(visionaiflow::domain::JobState::Created, visionaiflow::domain::JobState::Validating).IsSuccess());
    QVERIFY(visionaiflow::domain::ValidateTransition(visionaiflow::domain::JobState::Running, visionaiflow::domain::JobState::Completing).IsSuccess());
    QVERIFY(visionaiflow::domain::ValidateTransition(visionaiflow::domain::JobState::Completing, visionaiflow::domain::JobState::Completed).IsSuccess());
}

void JobStateTest::RejectsInvalidTransition()
{
    const auto result = visionaiflow::domain::ValidateTransition(visionaiflow::domain::JobState::Created, visionaiflow::domain::JobState::Running);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

void JobStateTest::IdentifiesTerminalStates()
{
    QVERIFY(visionaiflow::domain::IsTerminal(visionaiflow::domain::JobState::Completed));
    QVERIFY(visionaiflow::domain::IsTerminal(visionaiflow::domain::JobState::Failed));
    QVERIFY(!visionaiflow::domain::IsTerminal(visionaiflow::domain::JobState::Running));
}

QTEST_APPLESS_MAIN(JobStateTest)

#include "tst_JobState.moc"
