#include "core/AppPreferences.h"
#include "core/Label.h"
#include "core/LabelPlusDocument.h"

#include <QDir>
#include <QFile>
#include <QKeySequence>
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

    void preferencesReadMarkerFloatingPointSizes()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_preferences_test");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath("preference.json");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << "{\n"
               << "  \"appearance\": {\n"
               << "    \"style\": \"Fusion\",\n"
               << "    \"theme\": \"breezeDark\"\n"
               << "  },\n"
               << "  \"labelMarker\": {\n"
               << "    \"diameter\": 4.5,\n"
               << "    \"fontPointSize\": 2.5\n"
               << "  },\n"
               << "  \"labelTable\": {\n"
               << "    \"fontFamily\": \"Noto Serif CJK SC\",\n"
               << "    \"fontPointSize\": 11.5,\n"
               << "    \"maxTextRows\": 4\n"
               << "  },\n"
               << "  \"labelTextEditor\": {\n"
               << "    \"fontFamily\": \"Noto Sans Mono\",\n"
               << "    \"fontPointSize\": 12.5\n"
               << "  },\n"
               << "  \"markerTextBubble\": {\n"
               << "    \"fontFamily\": \"Noto Sans CJK SC\",\n"
               << "    \"fontPointSize\": 9.5\n"
               << "  },\n"
               << "  \"input\": {\n"
               << "    \"moveLabelModifier\": \"ctrl+shift\",\n"
               << "    \"nextLabelShortcut\": \"Tab\",\n"
               << "    \"editLabelTextShortcut\": \"Return\",\n"
               << "    \"commitLabelTextShortcut\": \"Ctrl+Return\",\n"
               << "    \"undoShortcut\": \"Ctrl+Z\",\n"
               << "    \"redoShortcut\": \"Ctrl+Shift+Z\"\n"
               << "  },\n"
               << "  \"backupPath\": \"custom-bak\",\n"
               << "  \"backupIntervalSeconds\": 30,\n"
               << "  \"groupStyles\": [\n"
               << "    {\n"
               << "      \"groupColor\": \"#ff3835\",\n"
               << "      \"markerDiameter\": 4.5,\n"
               << "      \"fontPointSize\": 2.5,\n"
               << "      \"markerStyle\": \"circle\"\n"
               << "    },\n"
               << "    {\n"
               << "      \"groupColor\": \"#5ba8ec\",\n"
               << "      \"markerDiameter\": 5.5,\n"
               << "      \"fontPointSize\": 3.5,\n"
               << "      \"markerStyle\": \"square\"\n"
               << "    }\n"
               << "  ]\n"
               << "}\n";
        file.close();

        const auto result = labelminus::core::AppPreferences::loadFromFile(filePath);

        QVERIFY(result.warnings.isEmpty());
        QCOMPARE(result.preferences.labelMarkerDiameterPixels(), 4.5);
        QCOMPARE(result.preferences.labelMarkerFontPointSize(), 2.5);
        QCOMPARE(result.preferences.labelTableMaxTextRows(), 4);
        QCOMPARE(result.preferences.labelTableFontFamily(), QStringLiteral("Noto Serif CJK SC"));
        QCOMPARE(result.preferences.labelTableFontPointSize(), 11.5);
        QCOMPARE(result.preferences.labelTextEditorFontFamily(), QStringLiteral("Noto Sans Mono"));
        QCOMPARE(result.preferences.labelTextEditorFontPointSize(), 12.5);
        QCOMPARE(result.preferences.markerTextBubbleFontFamily(), QStringLiteral("Noto Sans CJK SC"));
        QCOMPARE(result.preferences.markerTextBubbleFontPointSize(), 9.5);
        QCOMPARE(result.preferences.applicationStyle(), QStringLiteral("Fusion"));
        QCOMPARE(result.preferences.applicationTheme(), QStringLiteral("breezeDark"));
        QCOMPARE(result.preferences.moveLabelModifiers(), Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(result.preferences.undoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Z"));
        QCOMPARE(result.preferences.redoShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Ctrl+Shift+Z"));
        QCOMPARE(result.preferences.nextLabelShortcut().toString(QKeySequence::PortableText), QStringLiteral("Tab"));
        QCOMPARE(result.preferences.editLabelTextShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Return"));
        QCOMPARE(result.preferences.commitLabelTextShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Ctrl+Return"));
        QCOMPARE(result.preferences.backupPath(), QStringLiteral("custom-bak"));
        QCOMPARE(result.preferences.backupIntervalSeconds(), 30);
        QCOMPARE(result.preferences.groupStyles().size(), 2);
        QCOMPARE(result.preferences.groupStyles().at(0).groupColor, QColor(QStringLiteral("#ff3835")));
        QCOMPARE(result.preferences.groupStyles().at(1).markerDiameter, 5.5);
        QCOMPARE(result.preferences.groupStyles().at(1).fontPointSize, 3.5);
        QVERIFY(result.preferences.groupStyles().at(1).markerShape == labelminus::core::MarkerShape::Square);
    }

    void preferencesWarnOnInvalidJson()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_preferences_test");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath("broken-preference.json");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("{");
        file.close();

        const auto result = labelminus::core::AppPreferences::loadFromFile(filePath);

        QVERIFY(!result.warnings.isEmpty());
        QCOMPARE(result.preferences.labelMarkerDiameterPixels(), 20.0);
        QCOMPARE(result.preferences.labelMarkerFontPointSize(), 10.0);
        QCOMPARE(result.preferences.labelTableMaxTextRows(), 3);
        QCOMPARE(result.preferences.labelTableFontFamily(), QString());
        QCOMPARE(result.preferences.labelTableFontPointSize(), 0.0);
        QCOMPARE(result.preferences.labelTextEditorFontFamily(), QString());
        QCOMPARE(result.preferences.labelTextEditorFontPointSize(), 0.0);
        QCOMPARE(result.preferences.markerTextBubbleFontFamily(), QString());
        QCOMPARE(result.preferences.markerTextBubbleFontPointSize(), 0.0);
        QCOMPARE(result.preferences.applicationStyle(), QString());
        QCOMPARE(result.preferences.applicationTheme(), QString());
        QCOMPARE(result.preferences.moveLabelModifiers(), Qt::ControlModifier);
        QCOMPARE(result.preferences.undoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Z"));
        QCOMPARE(result.preferences.redoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Y"));
        QCOMPARE(result.preferences.nextLabelShortcut().toString(QKeySequence::PortableText), QStringLiteral("Tab"));
        QCOMPARE(result.preferences.editLabelTextShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Return"));
        QCOMPARE(result.preferences.commitLabelTextShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Ctrl+Return"));
        QCOMPARE(result.preferences.backupPath(), QStringLiteral("bak"));
        QCOMPARE(result.preferences.backupIntervalSeconds(), 60);
    }
};

QTEST_GUILESS_MAIN(LabelTests)

#include "LabelTests.moc"
