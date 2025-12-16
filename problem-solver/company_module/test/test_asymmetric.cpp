#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>
#include "agent/construct_project_dag_agent.hpp"
#include "agent/topological_sort_agent.hpp"
#include "keynodes/scheduling_keynodes.hpp"
#include <vector>
#include <string>

using TopologicalSortAgentTest = ScMemoryTest;

class TopologicalSortTest : public TopologicalSortAgentTest
{
protected:
  void SetUp() override
  {
    ScMemoryTest::SetUp();
    m_ctx->SubscribeAgent<ConstructProjectDagAgent>();
    m_ctx->SubscribeAgent<TopologicalSortAgent>(); // ← твой агент
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

    // Должен быть создан сигнал: action_project_dag_ready -> project
    EXPECT_TRUE(m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::action_project_dag_ready, project, ScType::ConstPermPosArc));

    return project;
  }

  // Проверяем, что проект помечен как ацикличный (если твой агент так делает)
  bool IsProjectAcyclic(ScAddr const & project)
  {
    return m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::concept_acyclic_project, project, ScType::ConstPermPosArc);
  }

  // Проверяем, что проект помечен как цикличный
  bool IsProjectCyclic(ScAddr const & project)
  {
    return m_ctx->CheckConnector(
        ProjectSchedulingKeynodes::concept_cyclic_project, project, ScType::ConstPermPosArc);
  }
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

// Тест 1: ацикличный граф (успешная проверка)
TEST_F(TopologicalSortTest, AcyclicGraphIsAccepted)
{
  std::string csvData =
      "A;10;\n"
      "B;5;A\n"
      "C;7;A\n"
      "D;3;B,C";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  // Ждём, пока TopologicalSortAgent обработает событие
  ScWaiter waiter;
  waiter.Wait(500); // или используй ScAction, если агент завершает действие

  // Способ 1: если агент помечает проект
  EXPECT_TRUE(IsProjectAcyclic(project));
  EXPECT_FALSE(IsProjectCyclic(project));

  // Способ 2: если агент создаёт собственное действие — нужно искать его результат
  // (в этом тесте мы предполагаем маркировку через concept_*)
}

// Тест 2: цикличный граф (ошибка или маркировка как цикличный)
TEST_F(TopologicalSortTest, CyclicGraphIsRejected)
{
  std::string csvData =
      "A;10;B\n"  // A зависит от B
      "B;5;A";    // B зависит от A → цикл!

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(1000);

  // Ожидаем, что граф помечен как цикличный
  EXPECT_FALSE(IsProjectAcyclic(project));
  EXPECT_TRUE(IsProjectCyclic(project));
}

// Тест 3: одиночная задача (тривиально ациклична)
TEST_F(TopologicalSortTest, SingleTaskIsAcyclic)
{
  std::string csvData = "A;10;";

  ScAddr project = BuildProjectFromCSV(csvData);
  ASSERT_TRUE(project.IsValid());

  ScWaiter waiter;
  waiter.Wait(500);

  EXPECT_TRUE(IsProjectAcyclic(project));
}
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

    // Ищем дугу project -> tuple, помеченную как nrel_topological_order
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

    // Теперь извлекаем задачи из кортежа
    std::vector<std::string> actualOrder;
    ScIterator3Ptr itTasks = m_ctx->CreateIterator3(topologicalTuple, ScType::ConstCommonArc, ScType::ConstNode);
    while (itTasks->Next())
    {
        ScAddr task = itTasks->Get(2);
        // Получаем main_idtf
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

    ASSERT_EQ(actualOrder.size(), 4u);
    EXPECT_EQ(actualOrder[0], "A");
    EXPECT_EQ(actualOrder[3], "D");

    // B и C — на позициях 1 и 2 (в любом порядке)
    std::set<std::string> middle(actualOrder.begin() + 1, actualOrder.begin() + 3);
    EXPECT_EQ(middle, (std::set<std::string>{"B", "C"}));
}
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

    // 1. Находим topological order tuple
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

    // 2. Извлекаем задачи из кортежа
    std::map<std::string, ScAddr> tasks;

    ScIterator3Ptr itTasks = m_ctx->CreateIterator3(topologicalTuple, ScType::ConstCommonArc, ScType::Unknown);
    while (itTasks->Next())
    {
        ScAddr task = itTasks->Get(2);

        // Получаем main_idtf
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

    ASSERT_EQ(tasks.size(), 4u);

    // 3. Вспомогательная функция чтения
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

    // 4. Ожидаемые CPM значения
    struct Expected { uint32_t es, ef, ls, lf, slack; bool critical; };
    std::map<std::string, Expected> expected = {
        {"A",{0,5,0,5,0,true}},
        {"B",{5,8,6,9,1,false}},
        {"C",{5,9,5,9,0,true}},
        {"D",{9,11,9,11,0,true}}
    };

    for (auto const & [name, exp] : expected)
    {
        ASSERT_TRUE(tasks.count(name));
        ScAddr task = tasks[name];

        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_es), exp.es);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_ef), exp.ef);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_ls), exp.ls);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_lf), exp.lf);
        EXPECT_EQ(readAttr(task, ProjectSchedulingKeynodes::nrel_slack), exp.slack);

        bool isCritical = m_ctx->CheckConnector(
            ProjectSchedulingKeynodes::concept_crytical_task,
            task,
            ScType::ConstPermPosArc);

        EXPECT_EQ(isCritical, exp.critical);
    }
}

