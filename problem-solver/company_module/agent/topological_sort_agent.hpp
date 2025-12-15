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
    ScAddrVector GetTasks(ScAddr const & project);
    ScAddrVector GetDependencies(ScAddr const & task);
    bool IsAcyclic(ScAddrVector const & tasks);
    ScAddrVector TopologicalSort(ScAddrVector const & tasks);
};