#include <sc-memory/sc_keynodes.hpp>

// Создаём класс, объединяющий ключевые sc-элементы, используемые 
// агентами в рамках одного модуля.
// Наследуем его от базового класса ScKeynodes.
class SetProcessingKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_find_critical{
    "action_find_critical", ScType::ConstNodeClass};
  static inline ScKeynode const nrel_duration{
    "nrel_duration", ScType::ConstNodeNonRole};

  static inline ScKeynode const nrel_duration{
    "nrel_depends_on",ScType::ConstNodeNonRole
  };
  static inline ScKeynode const concept_task{
    "concept_task", ScType::ConstNodeClass};
  // Здесь первым аргументом конструктора является системный 
  // sc-идентификатор ключевого sc-элемента, а второй аргумент — 
  // тип этого sc-элемента.
  // Если в sc-памяти нет ключевого sc-элемента с таким системным 
  // sc-идентификатором, то в ней будет создан sc-элемент с таким 
  // системным sc-идентификатором и указанным типом.
};
