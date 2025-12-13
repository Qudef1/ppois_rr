#include "scheduling_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include "../keynodes/scheduling_keynodes.hpp"

ScAddr SchedulingAgent::GetActionClass() const
{
  return SchedulingKeynodes::action_analyze_schedule;
}

// Вспомогательная функция: обход в глубину для проверки цикла
bool hasCycleDFS(
    ScAddr const & node,
    std::unordered_map<ScAddr, std::vector<ScAddr>> const & graph,
    std::unordered_set<ScAddr> & visited,
    std::unordered_set<ScAddr> & recStack)
{
  visited.insert(node);
  recStack.insert(node);

  for (ScAddr const & neighbor : graph.at(node))
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

// Топологическая сортировка (Кан)
std::vector<ScAddr> topologicalSort(
    std::unordered_map<ScAddr, std::vector<ScAddr>> const & graph,
    std::unordered_map<ScAddr, int> inDegree)
{
  std::queue<ScAddr> q;
  std::vector<ScAddr> order;

  for (auto const & [node, deg] : inDegree)
  {
    if (deg == 0)
      q.push(node);
  }

  while (!q.empty())
  {
    ScAddr u = q.front();
    q.pop();
    order.push_back(u);

    for (ScAddr v : graph.at(u))
    {
      inDegree[v]--;
      if (inDegree[v] == 0)
        q.push(v);
    }
  }

  return order;
}

ScResult SchedulingAgent::DoProgram(ScAction & action)
{
  // Получаем аргумент — корневой узел плана (sc-структуру с задачами)
  auto const & [planAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(planAddr))
  {
    m_logger.Error("Plan not specified");
    return action.FinishWithError();
  }

  // Собираем все задачи и зависимости
  std::unordered_map<ScAddr, std::vector<ScAddr>> graph; // task -> depends_on
  std::unordered_map<ScAddr, int> inDegree;
  std::unordered_set<ScAddr> allTasks;

  // Ищем все задачи в плане (все исходящие связи — это либо зависимости, либо duration)
  ScIterator5Ptr it5 = m_context.CreateIterator5(
      planAddr,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      ScType::Unknown);
  while (it5->Next())
  {
    ScAddr const & task = it5->Get(2);
    if (!m_context.HelperCheckEdge(SchedulingKeynodes::concept_task, task, ScType::ConstPosArc))
      continue;

    allTasks.insert(task);
    graph[task] = {};
    inDegree[task] = 0;
  }

  // Собираем зависимости: task --nrel_dependens_on--> dep_task
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

      if (!m_context.HelperCheckEdge(SchedulingKeynodes::nrel_depends_on, arc, ScType::ConstPermPosArc))
        continue;

      if (allTasks.find(dep) != allTasks.end())
      {
        graph[dep].push_back(task); // dep → task (потому что task зависит от dep)
        inDegree[task]++;
      }
    }
  }

  // === Проверка циклов ===
  std::unordered_set<ScAddr> visited, recStack;
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

  // === Топологическая сортировка ===
  auto topoOrder = topologicalSort(graph, inDegree);
  if (topoOrder.size() != allTasks.size())
  {
    m_logger.Error("Topological sort failed: graph not fully connected");
    return action.FinishWithError();
  }

  // === Возвращаем результат ===
  ScStructure result = m_context.GenerateStructure();
  result << planAddr;

  // Добавляем порядок (можно как цепочку дуг rrel_1, rrel_2...)
  for (size_t i = 0; i < topoOrder.size(); ++i)
  {
    result << topoOrder[i];
  }

  // Добавим флаг успеха
  ScAddr success = m_context.CreateNode(ScType::ConstNode);
  m_context.CreateEdge(ScType::ConstPosArc, result, success);
  result << success;

  action.SetResult(result);
  m_logger.Info("Schedule analysis completed successfully");
  return action.FinishSuccessfully();
}