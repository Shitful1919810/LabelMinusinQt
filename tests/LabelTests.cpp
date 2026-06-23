#include "core/AppPreferences.h"
#include "core/Label.h"
#include "core/LabelPlusDocument.h"
#include "core/Project.h"
#include "services/LabelNavigator.h"
#include "services/ProjectMergeService.h"

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

    void labelNavigatorMovesAcrossVisibleLabels()
    {
        labelminus::core::Project project;
        project.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        project.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        project.images().last().labels.append(Label(QStringLiteral("a"), QStringLiteral("框内"), {}));
        project.images().last().labels.append(Label(QStringLiteral("b"), QStringLiteral("框外"), {}));
        project.images().append(labelminus::core::ImageEntry{QStringLiteral("002.png"), {}, {}});
        project.images().last().labels.append(Label(QStringLiteral("c"), QStringLiteral("框内"), {}));
        project.images().last().labels.append(Label(QStringLiteral("d"), QStringLiteral("框外"), {}));

        const QStringList visibleGroups{QStringLiteral("框内"), QStringLiteral("框外")};
        const auto next = labelminus::services::LabelNavigator::nextVisibleLabel(project, {0, 1, visibleGroups});
        QVERIFY(next.isValid());
        QCOMPARE(next.imageIndex, 1);
        QCOMPARE(next.labelIndex, 0);

        const auto previous =
            labelminus::services::LabelNavigator::previousVisibleLabel(project, {1, 0, visibleGroups});
        QVERIFY(previous.isValid());
        QCOMPARE(previous.imageIndex, 0);
        QCOMPARE(previous.labelIndex, 1);
    }

    void labelNavigatorRespectsVisibleGroups()
    {
        labelminus::core::Project project;
        project.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        project.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        project.images().last().labels.append(Label(QStringLiteral("a"), QStringLiteral("框外"), {}));
        project.images().last().labels.append(Label(QStringLiteral("b"), QStringLiteral("框内"), {}));
        project.images().append(labelminus::core::ImageEntry{QStringLiteral("002.png"), {}, {}});
        project.images().last().labels.append(Label(QStringLiteral("c"), QStringLiteral("框外"), {}));
        project.images().last().labels.append(Label(QStringLiteral("d"), QStringLiteral("框内"), {}));

        const QStringList visibleGroups{QStringLiteral("框内")};
        const auto next = labelminus::services::LabelNavigator::nextVisibleLabel(project, {0, 1, visibleGroups});
        QVERIFY(next.isValid());
        QCOMPARE(next.imageIndex, 1);
        QCOMPARE(next.labelIndex, 1);

        const auto previous =
            labelminus::services::LabelNavigator::previousVisibleLabel(project, {1, 1, visibleGroups});
        QVERIFY(previous.isValid());
        QCOMPARE(previous.imageIndex, 0);
        QCOMPARE(previous.labelIndex, 1);
    }

    void projectMergeUsesSingleInvolvedPageAutomatically()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_merge_single_test");
        QDir().mkpath(dirPath);

        labelminus::core::Project firstProject;
        firstProject.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        firstProject.images().append(labelminus::core::ImageEntry{QStringLiteral("002.png"), {}, {}});
        firstProject.images().last().labels.append(Label(QStringLiteral("second"), QStringLiteral("框外"), {}));
        firstProject.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        firstProject.images().last().labels.append(Label(QStringLiteral("first"), QStringLiteral("框内"), {}));

        labelminus::core::Project secondProject;
        secondProject.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        secondProject.images().append(labelminus::core::ImageEntry{QStringLiteral("003.png"), {}, {}});
        secondProject.images().last().labels.append(Label(QStringLiteral("third"), QStringLiteral("框外"), {}));

        const QString firstPath = QDir(dirPath).filePath("first.txt");
        const QString secondPath = QDir(dirPath).filePath("second.txt");
        LabelPlusDocument::saveToFile(firstProject, firstPath);
        LabelPlusDocument::saveToFile(secondProject, secondPath);

        const auto plan = labelminus::services::ProjectMergeService::createPlan({firstPath, secondPath});

        QVERIFY(plan.conflicts.isEmpty());
        QCOMPARE(plan.mergedProject.images().size(), 3);
        QCOMPARE(plan.mergedProject.images().at(0).name, QStringLiteral("001.png"));
        QCOMPARE(plan.mergedProject.images().at(1).name, QStringLiteral("002.png"));
        QCOMPARE(plan.mergedProject.images().at(2).name, QStringLiteral("003.png"));
        QCOMPARE(plan.mergedProject.images().at(0).labels.first().text(), QStringLiteral("first"));
        QCOMPARE(plan.mergedProject.images().at(1).labels.first().text(), QStringLiteral("second"));
        QCOMPARE(plan.mergedProject.images().at(2).labels.first().text(), QStringLiteral("third"));

        const QString mergedPath = QDir(dirPath).filePath("merged.txt");
        const labelminus::core::Project merged =
            labelminus::services::ProjectMergeService::mergedProjectWithSelections(plan, {}, mergedPath);
        QCOMPARE(merged.commentLines().size(), 4);
        QCOMPARE(merged.commentLines().first(), QStringLiteral("# LabelMinusMergeSources v2"));
        QCOMPARE(merged.commentLines().last(), QStringLiteral("# EndLabelMinusMergeSources"));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"firstImage\":\"001.png\"")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"lastImage\":\"002.png\"")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"pageCount\":2")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"labelCount\":2")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"sourceIndex\":1")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"sourcePath\":\"first.txt\"")));
        QVERIFY(!merged.commentLines().at(1).contains(dirPath));
        QVERIFY(merged.commentLines().at(2).contains(QStringLiteral("\"firstImage\":\"003.png\"")));
        QVERIFY(merged.commentLines().at(2).contains(QStringLiteral("\"lastImage\":\"003.png\"")));
        QVERIFY(merged.commentLines().at(2).contains(QStringLiteral("\"pageCount\":1")));
        QVERIFY(merged.commentLines().at(2).contains(QStringLiteral("\"sourceIndex\":2")));
        QVERIFY(merged.commentLines().at(2).contains(QStringLiteral("\"sourcePath\":\"second.txt\"")));
        QVERIFY(!merged.commentLines().at(2).contains(dirPath));
    }

    void projectMergeCreatesConflictForMultipleInvolvedProjects()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_merge_conflict_test");
        QDir().mkpath(dirPath);

        labelminus::core::Project firstProject;
        firstProject.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        firstProject.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        firstProject.images().last().labels.append(Label(QStringLiteral("first"), QStringLiteral("框内"), {}));

        labelminus::core::Project secondProject;
        secondProject.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        secondProject.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        secondProject.images().last().labels.append(Label(QStringLiteral("second"), QStringLiteral("框外"), {}));

        const QString firstPath = QDir(dirPath).filePath("first.txt");
        const QString secondPath = QDir(dirPath).filePath("second.txt");
        LabelPlusDocument::saveToFile(firstProject, firstPath);
        LabelPlusDocument::saveToFile(secondProject, secondPath);

        const auto plan = labelminus::services::ProjectMergeService::createPlan({firstPath, secondPath});

        QCOMPARE(plan.conflicts.size(), 1);
        QCOMPARE(plan.conflicts.first().candidates.size(), 2);

        const QString mergedPath = QDir(dirPath).filePath("merged.txt");
        const labelminus::core::Project merged =
            labelminus::services::ProjectMergeService::mergedProjectWithSelections(plan, {1}, mergedPath);
        QCOMPARE(merged.images().size(), 1);
        QCOMPARE(merged.images().first().labels.first().text(), QStringLiteral("second"));
        QCOMPARE(merged.commentLines().size(), 3);
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"firstImage\":\"001.png\"")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"lastImage\":\"001.png\"")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"sourceIndex\":2")));
        QVERIFY(merged.commentLines().at(1).contains(QStringLiteral("\"sourcePath\":\"second.txt\"")));
        QVERIFY(!merged.commentLines().at(1).contains(dirPath));
    }

    void labelPlusDocumentPreservesCommentLines()
    {
        const QString dirPath = QDir::temp().filePath("labelminus_comment_test");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath("translation.txt");

        labelminus::core::Project project;
        project.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
        project.setSourceName(QStringLiteral("source.zip"));
        project.setCommentLines({QStringLiteral("# LabelMinusMergeSources v1"),
                                 QStringLiteral("# {\"image\":\"001.png\",\"sourceIndex\":1}"),
                                 QStringLiteral("# EndLabelMinusMergeSources")});
        project.images().append(labelminus::core::ImageEntry{QStringLiteral("001.png"), {}, {}});
        project.images().last().labels.append(Label(QStringLiteral("text"), QStringLiteral("框内"), {}));

        LabelPlusDocument::saveToFile(project, filePath);

        const auto reloaded = LabelPlusDocument::loadFromFile(filePath);
        QCOMPARE(reloaded.sourceName(), QStringLiteral("source.zip"));
        QCOMPARE(reloaded.commentLines(), project.commentLines());
        QCOMPARE(reloaded.images().size(), 1);
        QCOMPARE(reloaded.images().first().labels.first().text(), QStringLiteral("text"));
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
               << "    \"fontPointSize\": 9.5,\n"
               << "    \"opacity\": 0.75\n"
               << "  },\n"
               << "  \"canvasLabelTextEditor\": {\n"
               << "    \"opacity\": 0.6\n"
               << "  },\n"
               << "  \"input\": {\n"
               << "    \"moveLabelModifier\": \"ctrl+shift\",\n"
               << "    \"previousLabelModifier\": \"ctrl\",\n"
               << "    \"nextLabelShortcut\": \"Tab\",\n"
               << "    \"previousPageShortcut\": \"Alt+Left\",\n"
               << "    \"nextPageShortcut\": \"Alt+Right\",\n"
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
        QCOMPARE(result.preferences.markerTextBubbleOpacity(), 0.75);
        QCOMPARE(result.preferences.canvasLabelTextEditorOpacity(), 0.6);
        const labelminus::core::AppPreferencesLoadResult serializedResult =
            labelminus::core::AppPreferences::loadFromJson(result.preferences.toJsonDocument().toJson());
        QVERIFY(serializedResult.warnings.isEmpty());
        QCOMPARE(serializedResult.preferences.markerTextBubbleOpacity(), 0.75);
        QCOMPARE(serializedResult.preferences.canvasLabelTextEditorOpacity(), 0.6);
        QCOMPARE(serializedResult.preferences.alternateNextLabelShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Ctrl+Down"));
        QCOMPARE(result.preferences.applicationStyle(), QStringLiteral("Fusion"));
        QCOMPARE(result.preferences.applicationTheme(), QStringLiteral("breezeDark"));
        QCOMPARE(result.preferences.moveLabelModifiers(), Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(result.preferences.previousLabelModifiers(), Qt::ControlModifier);
        QCOMPARE(result.preferences.undoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Z"));
        QCOMPARE(result.preferences.redoShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Ctrl+Shift+Z"));
        QCOMPARE(result.preferences.nextLabelShortcut().toString(QKeySequence::PortableText), QStringLiteral("Tab"));
        QCOMPARE(result.preferences.previousPageShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Alt+Left"));
        QCOMPARE(result.preferences.nextPageShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Alt+Right"));
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
        QCOMPARE(result.preferences.previousLabelModifiers(), Qt::ControlModifier);
        QCOMPARE(result.preferences.undoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Z"));
        QCOMPARE(result.preferences.redoShortcut().toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Y"));
        QCOMPARE(result.preferences.nextLabelShortcut().toString(QKeySequence::PortableText), QStringLiteral("Tab"));
        QCOMPARE(result.preferences.previousPageShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Alt+Left"));
        QCOMPARE(result.preferences.nextPageShortcut().toString(QKeySequence::PortableText),
                 QStringLiteral("Alt+Right"));
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
