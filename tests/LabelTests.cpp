#include "core/Label.h"

#include <QtTest/QtTest>

using labelminus::core::Label;

class LabelTests final : public QObject
{
    Q_OBJECT

private slots:
    void positionIsClamped()
    {
        Label label("text", "group", QPointF(-1.0, 2.0));

        QCOMPARE(label.position().x(), 0.0);
        QCOMPARE(label.position().y(), 1.0);
    }
};

QTEST_MAIN(LabelTests)

#include "LabelTests.moc"

