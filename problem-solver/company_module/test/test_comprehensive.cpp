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

using ComprehensiveTest = ScMemoryTest;

class ComprehensiveSchedulingTest : public ComprehensiveTest
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

  uint32_t ReadTaskAttr(ScAddr const & task, ScAddr const & rel)
  {
    ScIterator5Ptr it5 = m_ctx->CreateIterator5(
      task,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      rel);
    
    if (it5->Next())
    {
      std::string content;
      m_ctx->GetLinkContent(it5->Get(2), content);
      return std::stoul(content);
    }
    
    return 0;
  }

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


// Тест 1: Проверка топологического порядка
TEST_F(ComprehensiveSchedulingTest, TopologicalOrderIsValid)
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

  // Находим топологический порядок
  ScAddr topologicalTuple;
  ScIterator3Ptr it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
  while (it3->Next())
  {
    ScAddr arc = it3->Get(1);
    ScAddr tuple = it3->Get(2);
    
    if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_topological_order, arc, ScType::ConstPermPosArc))
    {
      topologicalTuple = tuple;
      break;
    }
  }
  
  ASSERT_TRUE(topologicalTuple.IsValid());
  
  // Извлекаем порядок задач
  std::vector<std::string> order;
  std::map<std::string, ScAddr> tasks = GetTasksFromProject(project);
  
  ScIterator3Ptr itTasks = m_ctx->CreateIterator3(topologicalTuple, ScType::ConstCommonArc, ScType::Unknown);
  while (itTasks->Next())
  {
    ScAddr task = itTasks->Get(2);
    
    for (auto const & [name, taskAddr] : tasks)
    {
      if (task == taskAddr)
      {
        order.push_back(name);
        break;
      }
    }
  }
  
  // Проверяем, что A стоит перед B и C
  auto pos_A = std::find(order.begin(), order.end(), "A");
  auto pos_B = std::find(order.begin(), order.end(), "B");
  auto pos_C = std::find(order.begin(), order.end(), "C");
  auto pos_D = std::find(order.begin(), order.end(), "D");
  
  ASSERT_NE(pos_A, order.end());
  ASSERT_NE(pos_B, order.end());
  ASSERT_NE(pos_C, order.end());
  ASSERT_NE(pos_D, order.end());
  
  EXPECT_LT(pos_A - order.begin(), pos_B - order.begin());
  EXPECT_LT(pos_A - order.begin(), pos_C - order.begin());
  EXPECT_LT(pos_B - order.begin(), pos_D - order.begin());
  EXPECT_LT(pos_C - order.begin(), pos_D - order.begin());
}


// Тест 2: Проверка проекта из постановки задачи (полный набор требований)
TEST_F(ComprehensiveSchedulingTest, FullExampleFromRequirements)
{
  // Проект из постановки: Анализ требований -> Проектирование -> Разработка -> Тестирование -> Развертывание
  std::string csvData =
      "A;14;\n"   // Анализ требований
      "B;21;A\n"  // Проектирование архитектуры
      "C;14;A\n"  // Дизайн интерфейса
      "D;35;B\n"  // Разработка backend
      "E;28;B,C\n" // Разработка frontend
      "F;21;D,E\n" // Тестирование
      "G;7;F";     // Развертывание

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1500);

  // Проверяем, что проект помечен как ацикличный
  bool isAcyclic = m_ctx->CheckConnector(
    ProjectSchedulingKeynodes::concept_acyclic_project,
    project,
    ScType::ConstPermPosArc);
  EXPECT_TRUE(isAcyclic);
  
  // Проверяем наличие топологического порядка
  ScAddr topologicalTuple;
  ScIterator3Ptr it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
  while (it3->Next())
  {
    ScAddr arc = it3->Get(1);
    ScAddr tuple = it3->Get(2);
    
    if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_topological_order, arc, ScType::ConstPermPosArc))
    {
      topologicalTuple = tuple;
      break;
    }
  }
  EXPECT_TRUE(topologicalTuple.IsValid());
  
  // Проверяем наличие критического пути
  ScAddr criticalPathTuple;
  it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
  while (it3->Next())
  {
    ScAddr arc = it3->Get(1);
    ScAddr tuple = it3->Get(2);
    
    if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_critical_path, arc, ScType::ConstPermPosArc))
    {
      criticalPathTuple = tuple;
      break;
    }
  }
  EXPECT_TRUE(criticalPathTuple.IsValid());
  
  // Проверяем минимальную продолжительность проекта
  uint32_t duration = ReadProjectDuration(project);
  EXPECT_EQ(duration, 98u); // A(14) + B(21) + D(35) + F(21) + G(7)
  
  // Проверяем, что все задачи имеют CPM атрибуты
  std::map<std::string, ScAddr> tasks = GetTasksFromProject(project);
  EXPECT_EQ(tasks.size(), 7u);
  
  for (auto const & [name, task] : tasks)
  {
    uint32_t es = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_es);
    uint32_t ef = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_ef);
    uint32_t ls = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_ls);
    uint32_t lf = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_lf);
    uint32_t slack = ReadTaskAttr(task, ProjectSchedulingKeynodes::nrel_slack);
    
    EXPECT_GE(es, 0u) << "Task " << name << " should have ES >= 0";
    EXPECT_GT(ef, 0u) << "Task " << name << " should have EF > 0";
    EXPECT_GE(slack, 0u) << "Task " << name << " should have slack >= 0";
  }
}


