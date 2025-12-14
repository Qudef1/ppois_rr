#include "scheduling_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include <map>
#include <set>
#include <queue>
#include <vector>
#include "../keynodes/scheduling_keynodes.hpp"

// Используем встроенный компаратор из SC-Memory
using ScAddrMap = std::map<ScAddr, std::vector<ScAddr>, ScAddrLessFunc>;
using ScAddrIntMap = std::map<ScAddr, int, ScAddrLessFunc>;
using ScAddrSet = std::set<ScAddr, ScAddrLessFunc>;

ScAddr SchedulingAgent::GetActionClass() const
{
    return SchedulingKeynodes::action_analyze_schedule;
}

bool hasCycleDFS(
    ScAddr const & node,
    ScAddrMap const & graph,
    ScAddrSet & visited,
    ScAddrSet & recStack)
{
    visited.insert(node);
    recStack.insert(node);

    auto it = graph.find(node);
    if (it == graph.end()) return false;

    for (ScAddr const & neighbor : it->second)
    {
        if (visited.find(neighbor) == visited.end())
        {
            if (hasCycleDFS(neighbor, graph, visited, recStack))
                return true;
        }
        else if (recStack.find(neighbor) != recStack.end())
        {
            return true;
        }
    }

    recStack.erase(node);
    return false;
}

std::vector<ScAddr> topologicalSort(
    ScAddrMap const & graph,
    ScAddrIntMap inDegree)
{
    std::queue<ScAddr> q;
    std::vector<ScAddr> order;

    for (const auto & kv : inDegree)
    {
        if (kv.second == 0)
            q.push(kv.first);
    }

    while (!q.empty())
    {
        ScAddr u = q.front();
        q.pop();
        order.push_back(u);

        auto it = graph.find(u);
        if (it != graph.end())
        {
            for (ScAddr v : it->second)
            {
                inDegree[v]--;
                if (inDegree[v] == 0)
                    q.push(v);
            }
        }
    }

    return order;
}

ScResult SchedulingAgent::DoProgram(ScAction & action)
{
    auto const & [planAddr] = action.GetArguments<1>();
    if (!m_context.IsElement(planAddr))
    {
        m_logger.Error("Plan not specified");
        return action.FinishWithError();
    }

    ScAddrMap graph;
    ScAddrIntMap inDegree;
    ScAddrSet allTasks;

    // Сбор всех задач в плане
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        planAddr,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        ScType::Unknown);
    while (it5->Next())
    {
        ScAddr const & task = it5->Get(2);
        if (!m_context.CheckConnector(SchedulingKeynodes::concept_task, task, ScType::ConstPosArc))
            continue;

        allTasks.insert(task);
        graph[task] = {};
        inDegree[task] = 0;
    }

    // Сбор зависимостей
    for (ScAddr const & task : allTasks)
    {
        ScIterator3Ptr depIt = m_context.CreateIterator3(
            task,
            ScType::ConstPermPosArc,
            ScType::ConstNode);
        while (depIt->Next())
        {
            ScAddr const & arc = depIt->Get(1);
            ScAddr const & dep = depIt->Get(2);

            if (!m_context.CheckConnector(SchedulingKeynodes::nrel_depends_on, arc, ScType::ConstPermPosArc))
                continue;

            if (allTasks.count(dep) > 0)
            {
                graph[dep].push_back(task); // dep → task
                inDegree[task]++;
            }
        }
    }

    // Проверка циклов
    ScAddrSet visited, recStack;
    bool hasCycle = false;
    for (ScAddr const & node : allTasks)
    {
        if (visited.find(node) == visited.end())
        {
            if (hasCycleDFS(node, graph, visited, recStack))
            {
                hasCycle = true;
                break;
            }
        }
    }

    if (hasCycle)
    {
        m_logger.Error("Cycle detected in task dependencies");
        return action.FinishWithError();
    }

    // Топологическая сортировка
    std::vector<ScAddr> topoOrder = topologicalSort(graph, inDegree);
    if (topoOrder.size() != allTasks.size())
    {
        m_logger.Error("Topological sort failed: graph not fully connected");
        return action.FinishWithError();
    }

    // Формируем результат
    ScStructure result = m_context.GenerateStructure();
    result << planAddr;
    for (ScAddr const & task : topoOrder)
    {
        result << task;
    }

    ScAddr success = m_context.GenerateNode(ScType::ConstNode);
    m_context.GenerateConnector(ScType::ConstPosArc, result, success);
    result << success;

    action.SetResult(result);
    m_logger.Info("Schedule analysis completed successfully");
    return action.FinishSuccessfully();
}