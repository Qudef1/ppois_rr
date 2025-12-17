#pragma once

#include <sc-memory/sc_agent.hpp>

using ConnectivityEvent = ScEventAfterGenerateOutgoingArc<ScType::ConstPermPosArc>;

class TopologicalSortAgent :public ScAgent<ConnectivityEvent>{
    public:
    //конструктор
    TopologicalSortAgent();
    //элемент появление которого инициирует работу агента
    ScAddr GetEventSubscriptionElement() const override;
    //шаблон условия запуска
    ScTemplate GetInitiationConditionTemplate(ConnectivityEvent const & event) const override;
    // действие для которого будет запускаться агент
    ScAddr GetActionClass() const override;
    // цикл программы агента
    ScResult DoProgram(ConnectivityEvent const & event, ScAction & action) override;
    private:
    // Получение всех задач проекта в векторе 
    ScAddrVector GetTasks(ScAddr const & project);
    // получение зависимостей конкретной задачи в векторе
    ScAddrVector GetDependencies(ScAddr const & task);
    // Проверка на ацикличность графа задач
    bool IsAcyclic(ScAddrVector const & tasks);
    // Топологическая сортировка задач
    ScAddrVector TopologicalSort(ScAddrVector const & tasks);
    // Получение продолжительности задачи
    uint32_t GetDuration(ScAddr const & task);
    // Построение зависимостей задач и вычисление атрибутов CPM
    void BuildTaskDependencies(ScAddrVector const & tasks, ScAddr const & project, std::map<std::string, uint32_t> & slackMap, uint32_t & projectDuration);
    // Получение дочерних задач конкретной задачи
    ScAddrVector GetChildren(ScAddr const & task);
    // Запись атрибута задачи в память
    void WriteAttr(ScAddr const & task, ScAddr const & nrel, uint32_t value);
    // Построение критического пути
    ScAddrVector BuildCriticalPath(ScAddrVector const & topoOrder, std::map<std::string, uint32_t> const & slack);
    // Поиск параллельных задач
    void FindParallelTasks(ScAddrVector const & topoOrder, std::map<std::string, uint32_t> const & ES, std::map<std::string, uint32_t> const & EF);
    // Сохранение продолжительности проекта
    void SaveProjectDuration(ScAddr const & project, uint32_t duration);
    // Получение main_idtf задачи
    std::string GetTaskMainIdtf(ScAddr const & task);
};