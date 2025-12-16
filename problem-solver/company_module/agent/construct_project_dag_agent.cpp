#include "construct_project_dag_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../keynodes/scheduling_keynodes.hpp"
#include <sstream>
#include <stdexcept>
#include <thread>
#include <chrono>

// Конструктор агента — логируем инициализацию
ConstructProjectDagAgent::ConstructProjectDagAgent()
{
  m_logger.Info("ConstructProjectDagAgent initialized.");
}

// Метод, возвращающий sc-узел действия, которое обрабатывает агент
ScAddr ConstructProjectDagAgent::GetActionClass() const
{
  return ProjectSchedulingKeynodes::action_construct_project_dag_from_csv;
}

// Генерация уникального системного идентификатора для задачи
int ConstructProjectDagAgent::GetNextSystemIdentifier(const std::string& baseName)
{
  int id = 0;
  while (m_context.SearchElementBySystemIdentifier(baseName + std::to_string(id)).IsValid())
  {
    ++id; // увеличиваем, пока существует элемент с таким идентификатором
  }
  return id;
}

// Создание задачи в sc-памяти с идентификатором и длительностью
void ConstructProjectDagAgent::CreateTask(ScAddr& taskNode, const std::string& taskId, uint32_t duration)
{
  // Генерируем уникальный системный идентификатор
  std::string sysId = "task_" + std::to_string(GetNextSystemIdentifier("task_"));

  // Создаём узел для задачи
  taskNode = m_context.GenerateNode(ScType::ConstNode);
  m_context.SetElementSystemIdentifier(sysId, taskNode);

  // Создаём link для main_idtf и связываем его с задачей
  ScAddr linkIdtf = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(linkIdtf, taskId);
  ScAddr arcIdtf = m_context.GenerateConnector(ScType::ConstCommonArc, taskNode, linkIdtf);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf, arcIdtf);

  // Помечаем узел как concept_task
  m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::concept_task, taskNode);

  // Создаём link для длительности задачи (nrel_duration)
  ScAddr durLink = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(durLink, std::to_string(duration));
  ScAddr durArc = m_context.GenerateConnector(ScType::ConstCommonArc, taskNode, durLink);
  m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_duration, durArc);

  // Логируем создание задачи
  m_logger.Info("Created task: ", taskId, " with duration ", duration);
}

// Утилита для разбиения строки на токены по разделителю
std::vector<std::string> ConstructProjectDagAgent::Split(const std::string& s, char delimiter)
{
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter))
  {
    // Убираем пробелы в начале и конце токена
    size_t start = token.find_first_not_of(" \t");
    size_t end = token.find_last_not_of(" \t");
    if (start != std::string::npos)
      tokens.push_back(token.substr(start, end - start + 1));
    else
      tokens.push_back("");
  }
  return tokens;
}

// Основная функция обработки действия агента
ScResult ConstructProjectDagAgent::DoProgram(ScAction& action)
{
  m_logger.Info("ConstructProjectDagAgent started processing action.");

  // 1. Читаем CSV из nrel_file_path, прикреплённого к действию
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

  // 2. Разбиваем CSV на строки и потом на части
  std::istringstream stream(csv);
  std::string line;
  std::vector<std::vector<std::string>> parsedLines;

  while (std::getline(stream, line))
  {
    if (line.empty()) continue;
    auto parts = Split(line, ';');  // разделяем по ';'
    if (parts.size() < 2) continue; // пропускаем некорректные строки
    parsedLines.push_back(parts);
  }

  m_logger.Info("Parsed ", parsedLines.size(), " lines from CSV.");

  // 3. Создаём все задачи и сохраняем их в m_taskMap
  m_taskMap.clear();
  for (auto& parts : parsedLines)
  {
    std::string id = parts[0];
    uint32_t duration = std::stoul(parts[1]);
    ScAddr task;
    CreateTask(task, id, duration);
    m_taskMap.emplace_back(id, task); // сохраняем пару <id, ScAddr>
  }

  m_logger.Info("Created ", m_taskMap.size(), " tasks.");

  // 4. Создаём зависимости между задачами
  for (auto& parts : parsedLines)
  {
    if (parts.size() < 3) continue;
    std::string id = parts[0];
    std::string depsStr = parts[2];
    if (depsStr.empty()) continue;

    auto depIds = Split(depsStr, ','); // список идентификаторов родительских задач

    // Находим child задачу
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
      // Находим parent задачу
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

      // Создаём дугу child -> parent и помечаем её как nrel_dependency
      ScAddr depArc = m_context.GenerateConnector(ScType::ConstCommonArc, child, parent);
      m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_dependency, depArc);
      m_logger.Info("Added dependency: ", id, " depends on ", depId);
    }
  }

  // 5. Создаём структуру проекта и добавляем туда задачи
  ScStructure project = m_context.GenerateStructure();
  for (auto& pair : m_taskMap)
  {
    project << pair.second; // добавляем задачу в структуру
  }

  m_logger.Info("Added tasks to project structure.");

  // 6. Сигнализируем, что DAG построен (генерируем событие)
  m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      ProjectSchedulingKeynodes::action_project_dag_ready,
      project);

  // Ждём, пока другой агент (например, TopologicalSortAgent) обработает DAG и пометит топологический порядок
  const int maxAttempts = 50; // ~500 мс
  bool foundMarked = false;
  for (int i = 0; i < maxAttempts; ++i)
  {
    ScIterator3Ptr it3 = m_context.CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
    while (it3->Next())
    {
      ScAddr arc = it3->Get(1);
      if (m_context.CheckConnector(ProjectSchedulingKeynodes::nrel_topological_order, arc, ScType::ConstPermPosArc))
      {
        foundMarked = true; // нашли пометку о топологическом порядке
        break;
      }
    }
    if (foundMarked) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // короткая пауза
  }

  m_logger.Info("DAG construction completed. Setting result. topological_mark_found=", foundMarked);

  // Возвращаем созданную структуру проекта как результат действия
  action.SetResult(project);
  return action.FinishSuccessfully();
}
