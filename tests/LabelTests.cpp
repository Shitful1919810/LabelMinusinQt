#include "core/Label.h"
#include "core/LabelPlusDocument.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtTest/QtTest>

using labelminus::core::Label;
using labelminus::core::LabelPlusDocument;

class LabelTests final : public QObject {
    Q_OBJECT

private slots:
    void positionIsClamped()
    {
        Label label("text", "group", QPointF(-1.0, 2.0));

        QCOMPARE(label.position().x(), 0.0);
        QCOMPARE(label.position().y(), 1.0);
    }

    void labelPlusDocumentRoundTrips()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_parser_test");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath("translation.txt");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << "1,0\n-\n框内\n框外\n-\n\n"
               << ">>>>>>>>[01.png]<<<<<<<<\n"
               << "----------------[1]----------------[0.170,0.344,2]\n"
               << "题目：憧憬的回忆\n\n";
        file.close();

        auto project = LabelPlusDocument::loadFromFile(filePath);

        QCOMPARE(project.groups().size(), 2);
        QCOMPARE(project.images().size(), 1);
        QCOMPARE(project.images().first().labels.size(), 1);
        QCOMPARE(project.images().first().labels.first().group(), QStringLiteral("框外"));

        LabelPlusDocument::saveToFile(project, filePath);
        auto reloaded = LabelPlusDocument::loadFromFile(filePath);
        QCOMPARE(reloaded.images().first().labels.first().text(), QStringLiteral("题目：憧憬的回忆"));
    }
};

QTEST_MAIN(LabelTests)

#include "LabelTests.moc"
