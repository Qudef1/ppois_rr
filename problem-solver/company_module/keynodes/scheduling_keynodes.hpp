#pragma once
#include <sc-memory/sc_keynodes.hpp>

class ProjectSchedulingKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_construct_project_dag_from_csv{
      "action_construct_project_dag_from_csv", ScType::ConstNodeClass};

  static inline ScKeynode const action_project_dag_ready{
      "action_project_dag_ready", ScType::ConstNodeClass};

  static inline ScKeynode const action_topological_check_dag{
    "action_topological_check_dag", ScType::ConstNodeClass};

  static inline ScKeynode const concept_task{
      "concept_task", ScType::ConstNodeClass};
  static inline ScKeynode const concept_crytical_task{
      "concept_crytical_task", ScType::ConstNodeClass};

  static inline ScKeynode const concept_acyclic_project{
      "concept_acyclic_project", ScType::ConstNodeClass};
  static inline ScKeynode const concept_cyclic_project{
      "concept_cyclic_project", ScType::ConstNodeClass};

  static inline ScKeynode const nrel_file_path{
      "nrel_file_path", ScType::ConstNodeNonRole};

  static inline ScKeynode const nrel_duration{
      "nrel_duration", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_topological_order{
    "nrel_topological_order", ScType::ConstNodeNonRole
  };
  static inline ScKeynode const nrel_dependency{
      "nrel_dependency", ScType::ConstNodeNonRole};

  // scheduling result relations

  static inline ScKeynode const nrel_es{
      "nrel_es", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_ef{
      "nrel_ef", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_ls{
      "nrel_ls", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_lf{
      "nrel_lf", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_slack{
      "nrel_slack", ScType::ConstNodeNonRole};
  
};