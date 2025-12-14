#include "scheduling_module.hpp"

#include "agent/construct_project_dag_agent.hpp"

SC_MODULE_REGISTER(SchedulingModule)
  ->Agent<ConstructProjectDagAgent>();