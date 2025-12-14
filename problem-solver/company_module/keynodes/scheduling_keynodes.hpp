#pragma once
#include <sc-memory/sc_keynodes.hpp>

class ProjectSchedulingKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_construct_project_dag_from_csv{
      "action_construct_project_dag_from_csv", ScType::ConstNodeClass};

  static inline ScKeynode const action_project_dag_ready{
      "action_project_dag_ready", ScType::ConstNodeClass};

  static inline ScKeynode const concept_task{
      "concept_task", ScType::ConstNodeClass};

  static inline ScKeynode const nrel_file_path{
      "nrel_file_path", ScType::ConstNodeNonRole};

  static inline ScKeynode const nrel_duration{
      "nrel_duration", ScType::ConstNodeNonRole};

  static inline ScKeynode const nrel_dependency{
      "nrel_dependency", ScType::ConstNodeNonRole};
};