#include "ui/controllers/SequenceEditorController.h"
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include "core/session/Session.h"
#include "core/workflow/BuiltinWorkflows.h"
#include "ui/dialogs/PromptSequenceEditor.h"
#include "ui/dialogs/SolutionTemplateViewer.h"
#include "ui/graph/SequenceGraphWidget.h"
#include "ui/widgets/StyledButton.h"

SequenceEditorController::SequenceEditorController(Session& session, SequenceGraphWidget* graph, QWidget* dialogParent)
    : m_session(session)
    , m_graph(graph)
    , m_dialogParent(dialogParent)
{
}

void SequenceEditorController::refreshGraph()
{
    m_session.workflows().refresh();
    if (!m_graph)
        return;
    m_graph->setSequenceManager(&m_session.workflows().sequences());
    m_graph->setTemplateManager(&m_session.workflows().templates());
    m_graph->rebuildGraph();
    m_graph->setTemplateSelectionByPath(m_templatePath);
}

void SequenceEditorController::redrawGraph()
{
    if (!m_graph)
        return;
    if (m_templatePath.isEmpty()) {
        m_graph->rebuildGraph();
    } else {
        m_graph->rebuildGraphForTemplate(m_templatePath);
        m_graph->setTemplateSelectionByPath(m_templatePath);
    }
}

void SequenceEditorController::setSelectedTemplate(const QString& templatePath)
{
    if (m_templatePath == templatePath)
        return;
    m_templatePath = templatePath;
    if (m_graph)
        m_graph->setTemplateSelectionByPath(m_templatePath);
}

void SequenceEditorController::highlightRunningSequence()
{
    const RunState& run = m_session.run();
    if (m_graph && run.isActive())
        m_graph->setCurrentSequence(run.sequence.name());
}

void SequenceEditorController::openEditorFor(const QString& sequenceName)
{
    const WorkflowLibrary::Found found = m_session.workflows().find(sequenceName, QString());
    if (!found.isValid()) {
        QMessageBox::warning(m_dialogParent, tr("Error"), tr("Sequence not found: %1").arg(sequenceName));
        return;
    }
    openEditorWindow(tr("Sequence editor: %1").arg(sequenceName), found.path, found.sequence);
}

void SequenceEditorController::openEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq)
{
    auto* win = new QWidget(nullptr);
    win->setWindowFlag(Qt::Window, true);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setWindowTitle(title);
    win->resize(1280, 820);
    win->setMinimumSize(1024, 700);

    auto* layout = new QVBoxLayout(win);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    auto* editor = new PromptSequenceEditor(win);
    editor->setSequenceManager(&m_session.workflows().sequences());
    editor->setSequence(seq);
    editor->setCurrentSequencePath(filePath);
    layout->addWidget(editor, 1);

    auto* buttons = new QHBoxLayout();
    auto* saveBtn = new StyledButton(tr("Save"), win);
    auto* closeBtn = new StyledButton(tr("Close"), win);
    saveBtn->setEnabled(false);
    buttons->addStretch(1);
    buttons->addWidget(saveBtn);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    QObject::connect(editor, &PromptSequenceEditor::sequenceChanged, win, [win, saveBtn](const PromptSequence& edited) {
        saveBtn->setEnabled(true);
        win->setWindowTitle(tr("Sequence editor: %1").arg(edited.name()));
    });
    QObject::connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, win]() {
        if (!m_session.workflows().sequences().save(editor->sequence(), filePath)) {
            QMessageBox::warning(win, tr("Error"), tr("Could not save the sequence."));
            return;
        }
        QMessageBox::information(win, tr("Saved"), tr("The sequence has been saved."));
        saveBtn->setEnabled(false);
        m_session.workflows().refresh();
        redrawGraph();
    });
    QObject::connect(closeBtn, &QPushButton::clicked, win, &QWidget::close);
    win->show();
}

void SequenceEditorController::editSequences()
{
    WorkflowLibrary& workflows = m_session.workflows();
    workflows.refresh();
    if (workflows.sequences().sequences().isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("No sequences"), tr("The folder has no *.seq.json files."));
        return;
    }
    QStringList names;
    QMap<QString, QString> pathByName;
    for (const QString& path : workflows.sequences().fileList()) {
        names << QFileInfo(path).fileName();
        pathByName.insert(names.last(), path);
    }
    bool ok = false;
    const QString name = QInputDialog::getItem(m_dialogParent, tr("Choose a sequence"), tr("File:"), names, 0, false, &ok);
    if (!ok || name.isEmpty())
        return;
    const QString path = pathByName.value(name);
    openEditorWindow(tr("Sequence editor: %1").arg(name), path, workflows.sequences().sequences().value(path));
}

void SequenceEditorController::createNewSequence()
{
    bool ok = false;
    const QString name = QInputDialog::getText(m_dialogParent, tr("New sequence"), tr("Name:"),
                                               QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    QMessageBox where(m_dialogParent);
    where.setWindowTitle(tr("Create Sequence"));
    where.setText(tr("Where would you like to create the sequence?"));
    QAbstractButton* inSequences = where.addButton(tr("In PromptSequences folder"), QMessageBox::ActionRole);
    QAbstractButton* inTemplate = where.addButton(tr("In Template folder"), QMessageBox::ActionRole);
    where.addButton(QMessageBox::Cancel);
    where.exec();

    WorkflowLibrary& workflows = m_session.workflows();
    if (where.clickedButton() == inSequences) {
        const QString path = QDir(workflows.sequences().root()).filePath(name + QStringLiteral(".seq.json"));
        PromptSequence seq = BuiltinWorkflows::defaultSequence();
        seq.setName(name);
        if (!workflows.sequences().save(seq, path)) {
            QMessageBox::warning(m_dialogParent, tr("Error"), tr("Cannot save sequence to: %1").arg(path));
            return;
        }
        workflows.refresh();
        QMessageBox::information(m_dialogParent, tr("OK"), tr("Sequence created:\n%1").arg(path));
    } else if (where.clickedButton() == inTemplate) {
        const QString dir = QFileDialog::getExistingDirectory(m_dialogParent, tr("Select Template Folder"),
                                                              workflows.templates().root());
        if (dir.isEmpty())
            return;
        if (!QFile::exists(QDir(dir).filePath(QStringLiteral("template.templ.json")))) {
            QMessageBox::warning(m_dialogParent, tr("Invalid Template Directory"),
                                 tr("The selected directory is not a valid template directory (missing template.templ.json)."));
            return;
        }
        if (!workflows.templates().createNewSequenceInTemplate(dir, name)) {
            QMessageBox::warning(m_dialogParent, tr("Error"), tr("Cannot create sequence in template directory: %1").arg(dir));
            return;
        }
        workflows.refresh();
        QMessageBox::information(m_dialogParent, tr("OK"),
                                 tr("Sequence created in template:\n%1").arg(dir + QLatin1Char('/') + name + QStringLiteral(".seq.json")));
    } else {
        return;
    }
    redrawGraph();
}

void SequenceEditorController::showTemplates()
{
    QDialog dialog(m_dialogParent);
    dialog.setWindowTitle(tr("Solution Templates"));
    dialog.resize(800, 600);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new SolutionTemplateViewer(&m_session.workflows().templates(), &dialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void SequenceEditorController::exportSequenceAsTemplate()
{
    WorkflowLibrary& workflows = m_session.workflows();
    workflows.refresh();
    const QMap<QString, PromptSequence>& all = workflows.sequences().sequences();
    if (all.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("No sequences"), tr("No sequences found."));
        return;
    }
    QStringList names;
    QMap<QString, QString> pathByName;
    for (const QString& path : workflows.sequences().fileList()) {
        names << QFileInfo(path).fileName();
        pathByName.insert(names.last(), path);
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(m_dialogParent, tr("Export"), tr("Choose:"), names, 0, false, &ok);
    if (!ok || chosen.isEmpty())
        return;
    const PromptSequence seq = all.value(pathByName.value(chosen));
    const QString name = QInputDialog::getText(m_dialogParent, tr("Name"), tr("Template name:"),
                                               QLineEdit::Normal, seq.name(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    QString description = QInputDialog::getMultiLineText(m_dialogParent, tr("Description"), tr("Description:"),
                                                         seq.description(), &ok);
    if (!ok)
        description = seq.description();
    const QString dir = QDir(workflows.templates().root()).absoluteFilePath(name);
    if (QDir(dir).exists()) {
        QMessageBox::warning(m_dialogParent, tr("Failed"), tr("A template with this name already exists."));
        return;
    }
    if (!workflows.templates().exportSequenceAsTemplate(pathByName.value(chosen), dir, name, description)) {
        QMessageBox::warning(m_dialogParent, tr("Failed"), tr("Export failed."));
        return;
    }
    workflows.refresh();
    QMessageBox::information(m_dialogParent, tr("OK"), tr("Exported."));
}
