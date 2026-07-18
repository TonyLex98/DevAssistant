#pragma once

#include <QString>
#include <PromptSequence.h>
#include <PromptSequenceManager.h>
#include <SolutionTemplateManager.h>
#include "../../services/IService.h"

// Контроллер редактора последовательностей
// Отвечает за:
//  - создание/редактирование/сохранение последовательностей
//  - управление графом последовательностей
//  - обновление кэша последовательностей из папок и шаблонов
class SequenceEditorController
{
public:
    SequenceEditorController(ISequenceService* seq, ITemplateService* tmpl, IGraphService* graph, IUIUpdater* ui);

    // Открытие графа последовательностей
    void openSequenceGraph();

    // Обновление подсветки текущей последовательности в графе
    void updateSequenceGraphHighlight();

    // Обновление выбора шаблона в графе
    void updateSequenceGraphTemplateSelection();

    // Открытие редактора для последовательности по имени
    void openSequenceEditorForName(const QString& sequenceName);

    // Диалог выбора и редактирования последовательности
    void editSequences();

    // Создание новой последовательности
    void createNewSequence();

    // Обновление кэша всех последовательностей (основные + из шаблонов)
    void refreshAllSequences();

    // Гарантия наличия default.seq.json
    void ensureDefaultSequence();

    // Обработка изменения выбора шаблона в чате
    void onChatTemplateSelectionChanged(const QString& templatePath);

    // Статический шаблон последовательности по умолчанию
    static PromptSequence defaultSequenceTemplate();

private:
    ISequenceService* m_seq;
    ITemplateService* m_tmpl;
    IGraphService* m_graph;
    IUIUpdater* m_ui;

    // Открытие окна редактора последовательности
    void openSequenceEditorWindow(const QString& title,
                                  const QString& filePath,
                                  const PromptSequence& seq);
};
