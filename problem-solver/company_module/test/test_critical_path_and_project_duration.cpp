#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>
#include "agent/construct_project_dag_agent.hpp"
#include "agent/topological_sort_agent.hpp"
#include "keynodes/scheduling_keynodes.hpp"
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>

using CriticalPathTest = ScMemoryTest;

class CriticalPathAndDurationTest : public CriticalPathTest
{
protected:
  void SetUp() override
  {
    ScMemoryTest::SetUp();
    m_ctx->SubscribeAgent<ConstructProjectDagAgent>();
    m_ctx->SubscribeAgent<TopologicalSortAgent>();
  }

  void TearDown() override
  {
    m_ctx->UnsubscribeAgent<TopologicalSortAgent>();
    m_ctx->UnsubscribeAgent<ConstructProjectDagAgent>();
    ScMemoryTest::TearDown();
  }

  // Создаёт проект из CSV и возвращает адрес структуры проекта
  ScAddr BuildProjectFromCSV(const std::string& csvData)
  {
    ScAction action = m_ctx->GenerateAction(ProjectSchedulingKeynodes::action_construct_project_dag_from_csv);
    
    ScAddr csvLink = m_ctx->GenerateLink(ScType::ConstNodeLink);
    m_ctx->SetLinkContent(csvLink, csvData);
    ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, csvLink);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_file_path, arc);

    EXPECT_TRUE(action.InitiateAndWait());
    EXPECT_TRUE(action.IsFinishedSuccessfully());

    ScStructure project = action.GetResult();
    EXPECT_FALSE(project.IsEmpty());
    
    return project;
  }

  // Получает задачи из проекта
  std::map<std::string, ScAddr> GetTasksFromProject(ScAddr const & project)
  {
    std::map<std::string, ScAddr> tasks;
    
    // Пробуем через ConstCommonArc
    ScIterator3Ptr it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::Unknown);
    while (it3->Next())
    {
      ScAddr task = it3->Get(2);
      
      // Проверяем, что это задача (не кортеж и не другие структуры)
      if (!m_ctx->CheckConnector(ProjectSchedulingKeynodes::concept_task, task, ScType::ConstPosArc))
        continue;
      
      ScIterator5Ptr idtfIt = m_ctx->CreateIterator5(
        task, ScType::ConstCommonArc, ScType::ConstNodeLink,
        ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
      
      if (idtfIt->Next())
      {
        std::string id;
        m_ctx->GetLinkContent(idtfIt->Get(2), id);
        tasks[id] = task;
      }
    }
    
    // Если не нашли через ConstCommonArc, пробуем через ConstPermPosArc
    if (tasks.empty())
    {
      it3 = m_ctx->CreateIterator3(project, ScType::ConstPermPosArc, ScType::Unknown);
      while (it3->Next())
      {
        ScAddr task = it3->Get(2);
        
        if (!m_ctx->CheckConnector(ProjectSchedulingKeynodes::concept_task, task, ScType::ConstPosArc))
          continue;
        
        ScIterator5Ptr idtfIt = m_ctx->CreateIterator5(
          task, ScType::ConstCommonArc, ScType::ConstNodeLink,
          ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
        
        if (idtfIt->Next())
        {
          std::string id;
          m_ctx->GetLinkContent(idtfIt->Get(2), id);
          tasks[id] = task;
        }
      }
    }
    
    return tasks;
  }

  // Читает числовой атрибут задачи
  uint32_t ReadTaskAttr(ScAddr const & task, ScAddr const & rel)
  {
    ScIterator5Ptr it5 = m_ctx->CreateIterator5(
      task,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      rel);
    
    EXPECT_TRUE(it5->Next()) << "Attribute not found";
    
    std::string content;
    m_ctx->GetLinkContent(it5->Get(2), content);
    return std::stoul(content);
  }

  // Находит критический путь в проекте
  ScAddr FindCriticalPathTuple(ScAddr const & project)
  {
    ScIterator3Ptr it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
    while (it3->Next())
    {
      ScAddr arc = it3->Get(1);
      ScAddr tuple = it3->Get(2);
      
      if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_critical_path, arc, ScType::ConstPermPosArc))
      {
        return tuple;
      }
    }
    return ScAddr();
  }

  // Извлекает задачи из кортежа
  std::vector<std::string> GetTasksFromTuple(ScAddr const & tuple)
  {
    std::vector<std::string> result;
    
    ScIterator3Ptr it3 = m_ctx->CreateIterator3(tuple, ScType::ConstCommonArc, ScType::Unknown);
    while (it3->Next())
    {
      ScAddr task = it3->Get(2);
      
      ScIterator5Ptr idtfIt = m_ctx->CreateIterator5(
        task, ScType::ConstCommonArc, ScType::ConstNodeLink,
        ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
      
      if (idtfIt->Next())
      {
        std::string id;
        m_ctx->GetLinkContent(idtfIt->Get(2), id);
        result.push_back(id);
      }
    }
    
    // Итератор возвращает элементы в обратном порядке добавления, переворачиваем
    std::reverse(result.begin(), result.end());
    
    return result;
  }

  // Читает продолжительность проекта
  uint32_t ReadProjectDuration(ScAddr const & project)
  {
    ScIterator5Ptr it5 = m_ctx->CreateIterator5(
      project,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      ProjectSchedulingKeynodes::nrel_duration);
    
    if (it5->Next())
    {
      std::string content;
      m_ctx->GetLinkContent(it5->Get(2), content);
      return std::stoul(content);
    }
    
    return 0;
  }
};

// Тест 1: Проверка критического пути для простого проекта
TEST_F(CriticalPathAndDurationTest, CriticalPathForSimpleProject)
{
  std::string csvData =
      "A;5;\n"
      "B;3;A\n"
      "C;4;A\n"
      "D;2;B,C";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1000);

  // Находим критический путь
  ScAddr criticalPathTuple = FindCriticalPathTuple(project);
  ASSERT_TRUE(criticalPathTuple.IsValid()) << "Critical path tuple not found";

  std::vector<std::string> criticalPath = GetTasksFromTuple(criticalPathTuple);
  
  // Критический путь должен содержать задачи A, C, D (slack=0)
  // A (0-5), C (5-9), D (9-11) - все критические
  ASSERT_GE(criticalPath.size(), 3u);
  
  // Проверяем, что A в начале пути (после реверса)
  EXPECT_EQ(criticalPath[0], "A") << "Critical path should start with A";
  
  // Проверяем, что D в конце пути
  EXPECT_EQ(criticalPath.back(), "D") << "Critical path should end with D";
  
  // Проверяем, что C в пути (между A и D)
  bool foundC = false;
  for (const std::string& task : criticalPath)
  {
    if (task == "C") foundC = true;
  }
  EXPECT_TRUE(foundC) << "Task C should be in critical path";
  
  // Проверяем, что путь содержит A, C, D в правильном порядке
  if (criticalPath.size() >= 3)
  {
    EXPECT_EQ(criticalPath[0], "A");
    EXPECT_EQ(criticalPath[1], "C");
    EXPECT_EQ(criticalPath[2], "D");
  }
}

// Тест 2: Проверка минимальной продолжительности проекта
TEST_F(CriticalPathAndDurationTest, ProjectDurationIsSaved)
{
  std::string csvData =
      "A;14;\n"
      "B;21;A\n"
      "C;14;A\n"
      "D;35;B\n"
      "E;28;B,C\n"
      "F;21;D,E\n"
      "G;7;F";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1500);

  // Читаем продолжительность проекта
  uint32_t projectDuration = ReadProjectDuration(project);
  EXPECT_GT(projectDuration, 0u) << "Project duration should be saved";
  
  // Проверяем, что продолжительность равна максимальному EF
  // A(14) -> B(21) -> D(35) -> F(21) -> G(7) = 14+21+35+21+7 = 98
  // или A(14) -> B(21) -> E(28) -> F(21) -> G(7) = 14+21+28+21+7 = 91
  // Самый длинный путь: A->B->D->F->G = 98
  EXPECT_EQ(projectDuration, 98u);
}

// Тест 3: Критический путь для проекта из примера (Вариант 2)
TEST_F(CriticalPathAndDurationTest, CriticalPathForExampleProject)
{
  // Проект из постановки задачи: A->B->D->F->G - критический путь
  std::string csvData =
      "A;14;\n"
      "B;21;A\n"
      "C;14;A\n"
      "D;35;B\n"
      "E;28;B,C\n"
      "F;21;D,E\n"
      "G;7;F";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1500);

  // Находим критический путь
  ScAddr criticalPathTuple = FindCriticalPathTuple(project);
  ASSERT_TRUE(criticalPathTuple.IsValid()) << "Critical path tuple not found";

  std::vector<std::string> criticalPath = GetTasksFromTuple(criticalPathTuple);
  
  // Критический путь должен быть A -> B -> D -> F -> G
  ASSERT_GE(criticalPath.size(), 5u);
  
  // Проверяем порядок критических задач
  EXPECT_EQ(criticalPath[0], "A");
  EXPECT_EQ(criticalPath[1], "B");
  EXPECT_EQ(criticalPath[2], "D");
  EXPECT_EQ(criticalPath[3], "F");
  EXPECT_EQ(criticalPath[4], "G");
}

// Тест 4: Проверка критического пути для проекта с одним критическим путём
TEST_F(CriticalPathAndDurationTest, CriticalPathSinglePath)
{
  // Линейный проект: A -> B -> C -> D (все задачи критические)
  std::string csvData =
      "A;10;\n"
      "B;5;A\n"
      "C;8;B\n"
      "D;3;C";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1000);

  ScAddr criticalPathTuple = FindCriticalPathTuple(project);
  ASSERT_TRUE(criticalPathTuple.IsValid());

  std::vector<std::string> criticalPath = GetTasksFromTuple(criticalPathTuple);
  
  // Все задачи должны быть в критическом пути
  EXPECT_EQ(criticalPath.size(), 4u);
  EXPECT_EQ(criticalPath[0], "A");
  EXPECT_EQ(criticalPath[1], "B");
  EXPECT_EQ(criticalPath[2], "C");
  EXPECT_EQ(criticalPath[3], "D");
  
  // Проверяем продолжительность проекта
  uint32_t duration = ReadProjectDuration(project);
  EXPECT_EQ(duration, 26u); // 10+5+8+3
}

// Тест 5: Проверка критического пути для проекта с несколькими начальными задачами
TEST_F(CriticalPathAndDurationTest, CriticalPathMultipleStarts)
{
  // Две независимые ветки, одна длиннее другой
  std::string csvData =
      "A;10;\n"
      "B;20;A\n"
      "C;5;\n"
      "D;8;C";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1000);

  ScAddr criticalPathTuple = FindCriticalPathTuple(project);
  ASSERT_TRUE(criticalPathTuple.IsValid());

  std::vector<std::string> criticalPath = GetTasksFromTuple(criticalPathTuple);
  
  // Критический путь должен быть A -> B (30 дней, длиннее чем C -> D = 13 дней)
  ASSERT_GE(criticalPath.size(), 2u);
  
  bool hasA = false, hasB = false;
  for (const std::string& task : criticalPath)
  {
    if (task == "A") hasA = true;
    if (task == "B") hasB = true;
  }
  
  EXPECT_TRUE(hasA && hasB) << "Critical path should contain A and B";
  
  uint32_t duration = ReadProjectDuration(project);
  EXPECT_EQ(duration, 30u); // A(10) + B(20)
}

// Тест 6: Проверка корректности критического пути через сравнение с вычисленной длительностью
TEST_F(CriticalPathAndDurationTest, CriticalPathDurationMatchesProjectDuration)
{
  std::string csvData =
      "A;14;\n"
      "B;21;A\n"
      "C;14;A\n"
      "D;35;B\n"
      "E;28;B,C\n"
      "F;21;D,E\n"
      "G;7;F";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1500);

  ScAddr criticalPathTuple = FindCriticalPathTuple(project);
  ASSERT_TRUE(criticalPathTuple.IsValid());

  std::vector<std::string> criticalPath = GetTasksFromTuple(criticalPathTuple);
  std::map<std::string, ScAddr> allTasks = GetTasksFromProject(project);
  
  // Суммируем длительности задач в критическом пути
  uint32_t criticalPathDuration = 0;
  for (const std::string& taskName : criticalPath)
  {
    ASSERT_TRUE(allTasks.count(taskName));
    ScAddr task = allTasks[taskName];
    
    // Читаем длительность задачи
    uint32_t duration = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_duration);
    criticalPathDuration += duration;
  }
  
  // Проверяем, что сумма длительностей критического пути равна общей продолжительности проекта
  uint32_t projectDuration = ReadProjectDuration(project);
  EXPECT_EQ(criticalPathDuration, projectDuration) 
    << "Critical path duration (" << criticalPathDuration 
    << ") should equal project duration (" << projectDuration << ")";
}

