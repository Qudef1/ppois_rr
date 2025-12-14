#pragma once

#include <sc-memory/sc_keynodes.hpp>

// Класс ключевых нодов для модуля планирования/расписания
class SchedulingKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_analyze_schedule{"action_analyze_schedule", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_depends_on{"nrel_depends_on", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_duration{"nrel_duration", ScType::ConstNodeNonRole};
  static inline ScKeynode const concept_task{"concept_task", ScType::ConstNodeClass};
};
