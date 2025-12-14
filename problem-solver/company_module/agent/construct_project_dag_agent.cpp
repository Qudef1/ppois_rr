#include "construct_project_dag_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../keynodes/scheduling_keynodes.hpp"
#include <sstream>
#include <stdexcept>

ConstructProjectDagAgent::ConstructProjectDagAgent()
{
  m_logger.Info("ConstructProjectDagAgent initialized.");
}

ScAddr ConstructProjectDagAgent::GetActionClass() const
{
  return ProjectSchedulingKeynodes::action_construct_project_dag_from_csv;
}

int ConstructProjectDagAgent::GetNextSystemIdentifier(const std::string& baseName)
{
  int id = 0;
  while (m_context.SearchElementBySystemIdentifier(baseName + std::to_string(id)).IsValid())
  {
    ++id;
  }
  return id;
}

void ConstructProjectDagAgent::CreateTask(ScAddr& taskNode, const std::string& taskId, uint32_t duration)
{
  std::string sysId = "task_" + std::to_string(GetNextSystemIdentifier("task_"));
  taskNode = m_context.GenerateNode(ScType::ConstNode);
  m_context.SetElementSystemIdentifier(sysId, taskNode);

  // main_idtf = taskId (например, "A")
  ScAddr linkIdtf = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(linkIdtf, taskId);
  ScAddr arcIdtf = m_context.GenerateConnector(ScType::ConstCommonArc, taskNode, linkIdtf);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf, arcIdtf);

  // concept_task
  m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::concept_task, taskNode);

  // nrel_duration
  ScAddr durLink = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(durLink, std::to_string(duration));
  ScAddr durArc = m_context.GenerateConnector(ScType::ConstCommonArc, taskNode, durLink);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_duration, durArc);

  m_logger.Info("Created task: ", taskId, " with duration ", duration);
}

std::vector<std::string> ConstructProjectDagAgent::Split(const std::string& s, char delimiter)
{
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter))
  {
    size_t start = token.find_first_not_of(" \t");
    size_t end = token.find_last_not_of(" \t");
    if (start != std::string::npos)
      tokens.push_back(token.substr(start, end - start + 1));
    else
      tokens.push_back("");
  }
  return tokens;
}

ScResult ConstructProjectDagAgent::DoProgram(ScAction& action)
{
  m_logger.Info("ConstructProjectDagAgent started processing action.");

  // 1. Читаем CSV из nrel_file_path
  ScIterator5Ptr it5 = m_context.CreateIterator5(
      action,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      ProjectSchedulingKeynodes::nrel_file_path);
  if (!it5->Next())
  {
    m_logger.Error("CSV content not found in action.");
    return action.FinishWithError();
  }
  ScAddr csvLink = it5->Get(2);
  std::string csv;
  m_context.GetLinkContent(csvLink, csv);

  // 2. Разбиваем на строки
  std::istringstream stream(csv);
  std::string line;
  std::vector<std::vector<std::string>> parsedLines;

  while (std::getline(stream, line))
  {
    if (line.empty()) continue;
    auto parts = Split(line, ';');
    if (parts.size() < 2) continue;
    parsedLines.push_back(parts);
  }

  m_logger.Info("Parsed ", parsedLines.size(), " lines from CSV.");

  // 3. Создаём все задачи
  m_taskMap.clear();
  for (auto& parts : parsedLines)
  {
    std::string id = parts[0];
    uint32_t duration = std::stoul(parts[1]);
    ScAddr task;
    CreateTask(task, id, duration);
    m_taskMap.emplace_back(id, task);
  }

  m_logger.Info("Created ", m_taskMap.size(), " tasks.");

  // 4. Создаём зависимости
  for (auto& parts : parsedLines)
  {
    if (parts.size() < 3) continue;
    std::string id = parts[0];
    std::string depsStr = parts[2];
    if (depsStr.empty()) continue;

    auto depIds = Split(depsStr, ',');

    // Находим child
    ScAddr child;
    for (auto& pair : m_taskMap)
    {
      if (pair.first == id)
      {
        child = pair.second;
        break;
      }
    }
    if (!child.IsValid()) continue;

    for (auto& depId : depIds)
    {
      // Находим parent
      ScAddr parent;
      for (auto& pair : m_taskMap)
      {
        if (pair.first == depId)
        {
          parent = pair.second;
          break;
        }
      }
      if (!parent.IsValid()) continue;

      // Создаём ориентированную связь: child зависит от parent
      // В sc-памяти: child --(nrel_dependency)--> parent
      ScAddr depArc = m_context.GenerateConnector(ScType::ConstCommonArc, child, parent);
      m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_dependency, depArc);
      m_logger.Info("Added dependency: ", id, " depends on ", depId);
    }
  }

  // 5. Создаём структуру проекта
  ScStructure project = m_context.GenerateStructure();

  // Добавляем задачи в структуру, как в CreateGraphAgent
  for (auto& pair : m_taskMap)
  {
    project << pair.second; // <-- Добавляем задачу в структуру
  }

  m_logger.Info("Added tasks to project structure.");

  // 6. Сигнализируем, что DAG готов
  m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      ProjectSchedulingKeynodes::action_project_dag_ready,
      project);

  m_logger.Info("DAG construction completed. Setting result.");
  action.SetResult(project);
  return action.FinishSuccessfully();
}