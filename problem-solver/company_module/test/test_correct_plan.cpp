#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>
#include "agent/construct_project_dag_agent.hpp"
#include "keynodes/scheduling_keynodes.hpp"
#include <vector>
#include <string>

using ConstructProjectDagTest = ScMemoryTest;

TEST_F(ConstructProjectDagTest, BuildProjectDAGFromCSV)
{
  m_ctx->SubscribeAgent<ConstructProjectDagAgent>();

  std::string csvData =
      "A;14;\n"
      "B;21;A\n"
      "C;14;A\n"
      "D;35;B\n"
      "E;28;B,C\n"
      "F;21;D,E\n"
      "G;7;F";

  ScAction action = m_ctx->GenerateAction(ProjectSchedulingKeynodes::action_construct_project_dag_from_csv);

  ScAddr csvLink = m_ctx->GenerateLink(ScType::ConstNodeLink);
  m_ctx->SetLinkContent(csvLink, csvData);
  ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, action, csvLink);
  m_ctx->GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_file_path, arc);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScStructure project = action.GetResult();
  EXPECT_FALSE(project.IsEmpty());

  // Проверка: action_project_dag_ready -> project
  EXPECT_TRUE(m_ctx->CheckConnector(
      ProjectSchedulingKeynodes::action_project_dag_ready, project, ScType::ConstPermPosArc));

  // Теперь ищем все задачи в sc-памяти, а не в структуре
  std::vector<std::string> expectedIds = {"A", "B", "C", "D", "E", "F", "G"};
  std::vector<uint32_t> expectedDurations = {14, 21, 14, 35, 28, 21, 7};

  std::vector<ScAddr> foundTasks;

  for (const std::string& id : expectedIds)
  {
    ScIterator5Ptr it5 = m_ctx->CreateIterator5(
        ScType::ConstNode,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        ScKeynodes::nrel_main_idtf);
    bool found = false;
    while (it5->Next())
    {
      ScAddr task = it5->Get(0);
      ScAddr link = it5->Get(2);
      std::string name;
      m_ctx->GetLinkContent(link, name);
      if (name == id)
      {
        // Проверяем, что это задача
        if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::concept_task, task, ScType::ConstPosArc))
        {
          foundTasks.push_back(task);
          found = true;
          break;
        }
      }
    }
    EXPECT_TRUE(found) << "Task " << id << " not found.";
  }

  EXPECT_EQ(foundTasks.size(), 7u);

  // Проверим длительности
  for (size_t i = 0; i < foundTasks.size(); ++i)
  {
    ScAddr task = foundTasks[i];
    ScIterator5Ptr it5 = m_ctx->CreateIterator5(
        task,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::nrel_duration);
    EXPECT_TRUE(it5->Next()) << "Duration for task " << expectedIds[i] << " not found.";
    std::string content;
    m_ctx->GetLinkContent(it5->Get(2), content);
    EXPECT_EQ(std::stoul(content), expectedDurations[i]);
  }

  // Проверим зависимости
  auto findTaskById = [&](const std::string& id) -> ScAddr
  {
    for (size_t i = 0; i < expectedIds.size(); ++i)
    {
      if (expectedIds[i] == id)
        return foundTasks[i];
    }
    return ScAddr();
  };

  auto hasDependency = [&](const std::string& childId, const std::string& parentId) -> bool
  {
    ScAddr child = findTaskById(childId);
    ScAddr parent = findTaskById(parentId);
    if (!child.IsValid() || !parent.IsValid()) return false;

    ScIterator3Ptr it3 = m_ctx->CreateIterator3(child, ScType::ConstCommonArc, parent);
    while (it3->Next())
    {
      ScAddr arc = it3->Get(1);
      if (m_ctx->CheckConnector(ProjectSchedulingKeynodes::nrel_dependency, arc, ScType::ConstPermPosArc))
      {
        return true;
      }
    }
    return false;
  };

  EXPECT_TRUE(hasDependency("B", "A"));
  EXPECT_TRUE(hasDependency("C", "A"));
  EXPECT_TRUE(hasDependency("D", "B"));
  EXPECT_TRUE(hasDependency("E", "B"));
  EXPECT_TRUE(hasDependency("E", "C"));
  EXPECT_TRUE(hasDependency("F", "D"));
  EXPECT_TRUE(hasDependency("F", "E"));
  EXPECT_TRUE(hasDependency("G", "F"));

  m_ctx->UnsubscribeAgent<ConstructProjectDagAgent>();
}