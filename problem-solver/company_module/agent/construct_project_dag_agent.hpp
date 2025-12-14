#pragma once
#include <sc-memory/sc_agent.hpp>
#include <string>
#include <vector>

class ConstructProjectDagAgent : public ScActionInitiatedAgent
{
public:
  ConstructProjectDagAgent();

  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  int GetNextSystemIdentifier(const std::string& baseName);
  void CreateTask(ScAddr& taskNode, const std::string& taskId, uint32_t duration);
  std::vector<std::string> Split(const std::string& s, char delimiter);
  ScAddr GetTaskByMainIdtf(const std::string& idtf);

  std::vector<std::pair<std::string, ScAddr>> m_taskMap; // ID -> ScAddr
};