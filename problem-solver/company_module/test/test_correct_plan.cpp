#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>
#include "../agent/scheduling_agent.hpp"
#include "../keynodes/scheduling_keynodes.hpp"

using RealisticPlanTest = ScMemoryTest;

TEST_F(RealisticPlanTest, RealisticProjectPlan_Success)
{
  // Регистрируем агент
  m_ctx->SubscribeAgent<SchedulingAgent>();

  // Создаём действие анализа расписания
  ScAction action = m_ctx->GenerateAction(SchedulingKeynodes::action_analyze_schedule);

  // Корень плана
  ScAddr plan = m_ctx->GenerateNode(ScType::ConstNode);

  // Создаём задачи A–G
  ScAddr taskA = m_ctx->GenerateNode(ScType::ConstNode); // Анализ требований
  ScAddr taskB = m_ctx->GenerateNode(ScType::ConstNode); // Проектирование архитектуры
  ScAddr taskC = m_ctx->GenerateNode(ScType::ConstNode); // Дизайн интерфейса
  ScAddr taskD = m_ctx->GenerateNode(ScType::ConstNode); // Backend
  ScAddr taskE = m_ctx->GenerateNode(ScType::ConstNode); // Frontend
  ScAddr taskF = m_ctx->GenerateNode(ScType::ConstNode); // Тестирование
  ScAddr taskG = m_ctx->GenerateNode(ScType::ConstNode); // Развертывание

  // Помечаем задачи как concept_task
  auto markAsTask = [&](ScAddr const & task)
  {
    m_ctx->GenerateConnector(ScType::ConstPosArc, SchedulingKeynodes::concept_task, task);
  };
  markAsTask(taskA);
  markAsTask(taskB);
  markAsTask(taskC);
  markAsTask(taskD);
  markAsTask(taskE);
  markAsTask(taskF);
  markAsTask(taskG);

  // Добавляем задачи в план
  auto addToPlan = [&](ScAddr const & task)
  {
    m_ctx->GenerateConnector(ScType::ConstCommonArc, plan, task);
  };
  addToPlan(taskA);
  addToPlan(taskB);
  addToPlan(taskC);
  addToPlan(taskD);
  addToPlan(taskE);
  addToPlan(taskF);
  addToPlan(taskG);

  // Функция для установки длительности задачи
  auto setDuration = [&](ScAddr const & task, uint32_t days)
  {
    ScAddr durationNode = m_ctx->GenerateNode(ScType::ConstNode);
    m_ctx->SetLinkContent(durationNode, std::to_string(days));

    ScAddr arc = m_ctx->GenerateConnector(ScType::ConstCommonArc, task, durationNode);
    m_ctx->GenerateConnector(ScType::ConstPosArc, SchedulingKeynodes::nrel_duration, arc);
  };

  // Устанавливаем длительности (в днях)
  setDuration(taskA, 14); // A
  setDuration(taskB, 21); // B
  setDuration(taskC, 14); // C
  setDuration(taskD, 35); // D
  setDuration(taskE, 28); // E
  setDuration(taskF, 21); // F
  setDuration(taskG, 7);  // G

  // Функция для добавления зависимости: child зависит от parent
  auto addDependency = [&](ScAddr const & child, ScAddr const & parent)
  {
    ScAddr depArc = m_ctx->GenerateConnector(ScType::ConstCommonArc, child, parent);
    m_ctx->GenerateConnector(ScType::ConstPosArc, SchedulingKeynodes::nrel_depends_on, depArc);
  };

  // Зависимости согласно варианту 2:
  addDependency(taskB, taskA); // B ← A
  addDependency(taskC, taskA); // C ← A
  addDependency(taskD, taskB); // D ← B
  addDependency(taskE, taskB); // E ← B
  addDependency(taskE, taskC); // E ← C
  addDependency(taskF, taskD); // F ← D
  addDependency(taskF, taskE); // F ← E
  addDependency(taskG, taskF); // G ← F

  // Устанавливаем аргумент действия — план
  action.SetArguments(plan);

  // Инициируем и ждём завершения
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // Проверяем, что результат есть
  ScStructure result = action.GetResult();
  EXPECT_FALSE(result.IsEmpty());
  EXPECT_TRUE(result.HasElement(plan));

  // Отписываем агент
  m_ctx->UnsubscribeAgent<SchedulingAgent>();
}