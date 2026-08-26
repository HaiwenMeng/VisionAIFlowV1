#include "visionaiflow/annotation/AnnotationDocument.h"

#include <QtTest>
#include <QUuid>

class AnnotationDocumentTest final : public QObject
{
    Q_OBJECT

private slots:
    void ReversesAddReplaceAndRemove();
    void TracksSavedRevisionAcrossUndoRedoAndReset();
};

void AnnotationDocumentTest::ReversesAddReplaceAndRemove()
{
    visionaiflow::annotation::AnnotationDocument document;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::annotation::Annotation initial{id, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-a"), {1.0, 2.0, 3.0, 4.0}, {}, {}, {}};
    const auto added = document.Add(initial);
    QVERIFY2(added.IsSuccess(), added.IsSuccess() ? "" : added.Failure().message.c_str());
    const visionaiflow::annotation::Annotation replacement{id, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-a"), {5.0, 6.0, 7.0, 8.0}, {}, {}, {}};
    QVERIFY(document.Replace(id, replacement).IsSuccess());
    QVERIFY(document.Remove(id).IsSuccess());
    QCOMPARE(static_cast<qsizetype>(document.Annotations().size()), 0);
    QVERIFY(document.Undo().IsSuccess());
    QCOMPARE(document.Annotations().front().boundingBox.x, 5.0);
    QVERIFY(document.Undo().IsSuccess());
    QCOMPARE(document.Annotations().front().boundingBox.x, 1.0);
    QVERIFY(document.Redo().IsSuccess());
    QCOMPARE(document.Annotations().front().boundingBox.x, 5.0);
    const visionaiflow::annotation::Annotation second{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-b"), {9.0, 10.0, 11.0, 12.0}, {}, {}, {}};
    QVERIFY(document.Add(second).IsSuccess());
    QVERIFY(!document.CanRedo());
}

void AnnotationDocumentTest::TracksSavedRevisionAcrossUndoRedoAndReset()
{
    visionaiflow::annotation::AnnotationDocument document;
    const QString firstId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString secondId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::annotation::Annotation first{firstId, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-a"), {1.0, 2.0, 3.0, 4.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation second{secondId, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-b"), {5.0, 6.0, 7.0, 8.0}, {}, {}, {}};

    QVERIFY(document.Reset({first}).IsSuccess());
    QVERIFY(!document.IsDirty());
    QVERIFY(!document.CanUndo());
    QVERIFY(!document.CanRedo());

    QVERIFY(document.Add(second).IsSuccess());
    QVERIFY(document.IsDirty());
    QVERIFY(document.CanUndo());
    document.MarkSaved();
    QVERIFY(!document.IsDirty());

    QVERIFY(document.Undo().IsSuccess());
    QCOMPARE(static_cast<qsizetype>(document.Annotations().size()), 1);
    QVERIFY(document.IsDirty());
    QVERIFY(document.Redo().IsSuccess());
    QCOMPARE(static_cast<qsizetype>(document.Annotations().size()), 2);
    QVERIFY(!document.IsDirty());

    const visionaiflow::annotation::Annotation invalid{firstId, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("label-a"), {1.0, 2.0, 3.0, 4.0}, {}, {}, {}};
    const auto duplicateReset = document.Reset({first, invalid});
    QVERIFY(!duplicateReset.IsSuccess());
    QVERIFY(!duplicateReset.Failure().message.empty());
}

QTEST_APPLESS_MAIN(AnnotationDocumentTest)

#include "tst_AnnotationDocument.moc"
