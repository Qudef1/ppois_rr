#include "scheduling_module.hpp"
#include "agent/scheduling_agent.hpp"

SC_MODULE_REGISTER(SchedulingModule)
  ->Agent<SchedulingAgent>();