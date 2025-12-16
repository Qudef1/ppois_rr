#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>
#include "agent/construct_project_dag_agent.hpp"
#include "agent/topological_sort_agent.hpp"
#include "keynodes/scheduling_keynodes.hpp"
#include <vector>
#include <string>

// Базовый класс для тестов с использованием sc-memory
using TopologicalSortAgentTest = ScMemoryTest;

// Класс тестов для TopologicalSortAgent
class TopologicalSortTest : public TopologicalSortAgentTest
{
protected:
  // Выполняется перед каждым тестом
  void SetUp() override
  {
    ScMemoryTest::SetUp();
    m_ctx->SubscribeAgent<ConstructProjectDagAgent>(); // подписываем агента, который строит DAG
    m_ctx->SubscribeAgent<TopologicalSortAgent>();      // подписываем агент TopologicalSortAgent
  }

  // Выполняется после каждого теста
  void TearDown() override
  {
    m_ctx->UnsubscribeAgent<TopologicalSortAgent>();
    m_ctx->UnsubscribeAgent<ConstructProjectDagAgent>();
    ScMemoryTest::TearDown();
  }

  // Вспомогательная функция: создаёт проект из CSV и возвращает структуру
  ScAddr BuildProjectFromCSV(const std::string& csvData)
  {
    // Генерация действия
    ScAction action = m_ctx->GenerateAction(ProjectSchedulingKeynodes::action_construct_project_dag_from_csv);

    // Создаём link с CSV содержимым
    ScAddr csvLink = m_ctx->GenerateLink(ScType::ConstNodeLink);
    m_ctx->SetLinkContent(csvLink, csvData);

    // Связываем action с csvLink
    ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, csvLink);

    // Помечаем связь как nrel_file_path
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_file_path, arc);

    // Инициация действия и ожидание завершения
    EXPECT_TRUE(action.InitiateAndWait());
    EXPECT_TRUE(action.IsFinishedSuccessfully());

    // Получаем результат (структуру проекта)
    ScStructure project = action.GetResult();
    EXPECT_FALSE(project.IsEmpty());

    // Проверяем, что DAG готов: action_project_dag_ready -> project
    EXPECT_TRUE(m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::action_project_dag_ready, project, ScType::ConstPermPosArc));

    return project;
  }

  // Проверка, что проект помечен как ацикличный
  bool IsProjectAcyclic(ScAddr const & project)
  {
    return m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::concept_acyclic_project, project, ScType::ConstPermPosArc);
  }

  // Проверка, что проект помечен как цикличный
  bool IsProjectCyclic(ScAddr const & project)
  {
    return m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::concept_cyclic_project, project, ScType::ConstPermPosArc);
  }

  // Вспомогательная функция для получения числового атрибута задачи
  uint32_t GetTaskAttr(ScAddr const & task, ScAddr const & rel)
  {
      ScIterator5Ptr it5 = m_ctx->CreateIterator5(
          task,
          ScType::ConstCommonArc,
          ScType::ConstNodeLink,
          ScType::ConstPermPosArc,
          rel);

      EXPECT_TRUE(it5->Next()) << "Attribute not found for task";

      std::string content;
      m_ctx->GetLinkContent(it5->Get(2), content);
      return std::stoul(content);
  }
};

// ================================
// Тест 1: ацикличный граф
// ================================
TEST_F(TopologicalSortTest, AcyclicGraphIsAccepted)
{
  std::string csvData =
      "A;10;\n"
      "B;5;A\n"
      "C;7;A\n"
      "D;3;B,C";

  // Создаём проект
  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  // Ждём 500 мс, пока TopologicalSortAgent обработает DAG
  ScWaiter waiter;
  waiter.Wait(500);

  // Проверяем маркировку проекта
  EXPECT_TRUE(IsProjectAcyclic(project));
  EXPECT_FALSE(IsProjectCyclic(project));
}

// ================================
// Тест 2: цикличный граф
// ================================
TEST_F(TopologicalSortTest, CyclicGraphIsRejected)
{
  std::string csvData =
      "A;10;B\n"  // A зависит от B
      "B;5;A";    // B зависит от A → цикл!

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1000);

  // Проверяем, что граф помечен как цикличный
  EXPECT_FALSE(IsProjectAcyclic(project));
  EXPECT_TRUE(IsProjectCyclic(project));
}

// ================================
// Тест 3: одиночная задача
// ================================
TEST_F(TopologicalSortTest, SingleTaskIsAcyclic)
{
  std::string csvData = "A;10;";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(500);

  EXPECT_TRUE(IsProjectAcyclic(project));
}

// ================================
// Тест 4: корректный топологический порядок
// ================================
TEST_F(TopologicalSortTest, TopologicalOrderIsCorrect)
{
    std::string csvData =
        "A;5;\n"
        "B;3;A\n"
        "C;4;A\n"
        "D;2;B,C";

    ScAddr project = BuildProjectFromCSV(csvData);
    ASSERT_TRUE(project.IsValid());

    ScAddr topologicalTuple;
    bool found = false;

    // Ищем кортеж с топологическим порядком: project -> tuple
    ScIterator3Ptr it3 = m_ctx->CreateIterator3(project, ScType::ConstCommonArc, ScType::ConstNodeTuple);
    while (it3->Next())
    {
        ScAddr arc = it3->Get(1);
        ScAddr tuple = it3->Get(2);

        if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_topological_order, arc, ScType::ConstPermPosArc))
        {
            topologicalTuple = tuple;
            found = true;
            break;
        }
    }

    ASSERT_TRUE(found) << "Topological order tuple not found";

    // Извлекаем задачи из кортежа
    std::vector<std::string> actualOrder;
    ScIterator3Ptr itTasks = m_ctx->CreateIterator3(topologicalTuple, ScType::ConstCommonArc, ScType::ConstNode);
    while (itTasks->Next())
    {
        ScAddr task = itTasks->Get(2);

        // Получаем идентификатор задачи (main_idtf)
        ScIterator5Ptr idtfIt = m_ctx->CreateIterator5(
            task, ScType::ConstCommonArc, ScType::ConstNodeLink,
            ScType::ConstPermPosArc, ScKeynodes::nrel_main_idtf);
        if (idtfIt->Next())
        {
            std::string id;
            m_ctx->GetLinkContent(idtfIt->Get(2), id);
            actualOrder.push_back(id);
        }
    }

    // Проверяем порядок: A первая, D последняя
    ASSERT_EQ(actualOrder.size(), 4u);
    EXPECT_EQ(actualOrder[0], "A");
    EXPECT_EQ(actualOrder[3], "D");

    // B и C могут идти в любом порядке на позициях 1 и 2
    std::set<std::string> middle(actualOrder.begin() + 1, actualOrder.begin() + 3);
    EXPECT_EQ(middle, (std::set<std::string>{"B", "C"}));
}

// ================================
// Тест 5: проверка ES, EF, LS, LF и Slack
// ================================
TEST_F(TopologicalSortTest, CPMAttributesAreCorrect)
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

    // Находим topological order tuple
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

    // Извлекаем задачи из кортежа
    std::map<std::string, ScAddr> tasks;
    ScIterator3Ptr itTasks = m_ctx->CreateIterator3(topologicalTuple, ScType::ConstCommonArc, ScType::Unknown);
    while (itTasks->Next())
    {
        ScAddr task = itTasks->Get(2);

        // Получаем main_idtf задачи
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

    // Должно быть 4 задачи
    ASSERT_EQ(tasks.size(), 4u);

    // Вспомогательная функция для чтения числовых атрибутов
    auto readAttr = [&](ScAddr task, ScAddr rel) -> uint32_t
    {
        ScIterator5Ptr it = m_ctx->CreateIterator5(
            task,
            ScType::ConstCommonArc,
            ScType::ConstNodeLink,
            ScType::ConstPermPosArc,
            rel);

        EXPECT_TRUE(it->Next());

        std::string value;
        m_ctx->GetLinkContent(it->Get(2), value);
        return std::stoul(value);
    };

    // Ожидаемые значения CPM
    struct Expected { uint32_t es, ef, ls, lf, slack; bool critical; };
    std::map<std::string, Expected> expected = {
        {"A",{0,5,0,5,0,true}},
        {"B",{5,8,6,9,1,false}},
        {"C",{5,9,5,9,0,true}},
        {"D",{9,11,9,11,0,true}}
    };

    // Проверяем каждую задачу на соответствие атрибутов CPM
    for (auto const & [name, exp] : expected)
    {
        ASSERT_TRUE(tasks.count(name));
        ScAddr task = tasks[name];

        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_es), exp.es);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_ef), exp.ef);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_ls), exp.ls);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_lf), exp.lf);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_slack), exp.slack);

        // Проверяем, что критические задачи помечены
        bool isCritical = m_ctx->CheckConnector(
            ProjectSchedulingKeynodes::concept_crytical_task,
            task,
            ScType::ConstPermPosArc);

        EXPECT_EQ(isCritical, exp.critical);
    }
}
