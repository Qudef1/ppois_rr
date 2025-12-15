#include "topological_sort_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../keynodes/scheduling_keynodes.hpp"

TopologicalSortAgent::TopologicalSortAgent(){
  m_logger.Info("Topological sort agent initialized.");
}

ScAddr TopologicalSortAgent::GetEventSubscriptionElement() const{
  return ProjectSchedulingKeynodes::action_project_dag_ready;
}

ScAddr TopologicalSortAgent::GetActionClass() const{
  return ProjectSchedulingKeynodes::action_topological_check_dag;
}
ScTemplate TopologicalSortAgent::GetInitiationConditionTemplate(ConnectivityEvent const & event) const
{
  ScTemplate templ;
  templ.Triple(ProjectSchedulingKeynodes::action_project_dag_ready, ScType::VarPermPosArc, ScType::VarNode);
  return templ;
}
ScAddrVector TopologicalSortAgent::GetTasks(ScAddr const & project){
    // Try to collect tasks as any elements pointed by common arcs from project
    ScAddrVector tasks;

    ScIterator3Ptr it3_any = m_context.CreateIterator3(project, ScType::ConstCommonArc, ScType::Unknown);
    while (it3_any->Next())
    {
        ScAddr const & elementAddr = it3_any->Get(2);
        tasks.push_back(elementAddr);
    }

    // If nothing found, try PermPosArc variant (some generators may use different types)
    if (tasks.empty())
    {
        ScIterator3Ptr it3_perm = m_context.CreateIterator3(project, ScType::ConstPermPosArc, ScType::Unknown);
        while (it3_perm->Next())
        {
            ScAddr const & elementAddr = it3_perm->Get(2);
            tasks.push_back(elementAddr);
        }
    }

    return tasks;
}
ScAddrVector TopologicalSortAgent::GetDependencies(ScAddr const & task){
    ScAddrVector dependencies;
    // We search for common arcs outgoing from the child task to its parent tasks
    // which are marked by a perm pos arc from nrel_dependency to that common arc.
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        task,
        ScType::ConstCommonArc,    // common arc from child -> parent
        ScType::ConstNode,         // parent node
        ScType::ConstPermPosArc,   // marking arc
        ProjectSchedulingKeynodes::nrel_dependency);

    while (it5->Next())
    {
        ScAddr parent = it5->Get(2); // target of common arc is the parent
        dependencies.push_back(parent);
    }
    return dependencies;
}
bool TopologicalSortAgent::IsAcyclic(ScAddrVector const & tasks)
{
    // 1. Преобразуем ScAddr → system identifier (string)
    std::map<std::string, ScAddr> nameToAddr;
    std::set<std::string> taskNames;

    for (const ScAddr& task : tasks)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (!name.empty())
        {
            nameToAddr[name] = task;
            taskNames.insert(name);
        }
    }

    // 2. Цвета по имени
    std::map<std::string, int> color; // 0=white, 1=gray, 2=black
    for (const std::string& name : taskNames)
    {
        color[name] = 0;
    }

    // 3. DFS по именам
    for (const std::string& startName : taskNames)
    {
        if (color[startName] != 0) continue;

        std::stack<std::string> stack;
        stack.push(startName);

        while (!stack.empty())
        {
            std::string u = stack.top();

            if (color[u] == 0)
            {
                color[u] = 1; // gray

                // Получаем ScAddr по имени
                ScAddr uAddr = nameToAddr[u];

                // Получаем зависимости: ScAddr → vector<ScAddr>
                std::vector<ScAddr> deps = GetDependencies(uAddr);

                for (const ScAddr& vAddr : deps)
                {
                    std::string v = m_context.GetElementSystemIdentifier(vAddr);
                    if (v.empty()) continue;

                    // Только если v — часть проекта
                    if (taskNames.find(v) == taskNames.end())
                        continue;

                    if (color[v] == 0)
                    {
                        stack.push(v);
                    }
                    else if (color[v] == 1)
                    {
                        // Цикл!
                        return false;
                    }
                }
            }
            else if (color[u] == 1)
            {
                color[u] = 2; // black
                stack.pop();
            }
            else
            {
                stack.pop();
            }
        }
    }

    return true;
}
ScAddrVector TopologicalSortAgent::TopologicalSort(ScAddrVector const & tasks){
    std::map<std::string, ScAddr> nameToAddr;
    std::map<std::string, std::vector<std::string>> children; // parent_name → [child_names]
    std::map<std::string, int> inDegree;

    for(const auto & task : tasks){
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (name.empty()) continue;

        nameToAddr[name] = task;
        inDegree[name] = 0;
        children[name] = std::vector<std::string>();
    }
    for (const ScAddr& childAddr : tasks)
    {
        std::string childName = m_context.GetElementSystemIdentifier(childAddr);
        if (childName.empty()) continue;

        auto parentAddrs = GetDependencies(childAddr); // returns parents (child зависит от них)

        for (const ScAddr& parentAddr : parentAddrs)
        {
            std::string parentName = m_context.GetElementSystemIdentifier(parentAddr);
            if (parentName.empty()) continue;
            if (nameToAddr.find(parentName) == nameToAddr.end()) continue;

            // parent → child
            children[parentName].push_back(childName);
            inDegree[childName]++; // child зависит от parent → in-degree++
        }
    }
    std::queue<std::string> zeroInDegree;
    for (const auto& p : inDegree)
    {
        if (p.second == 0)
        {
            zeroInDegree.push(p.first);
        }
    }

    std::vector<std::string> topoOrder;
    while (!zeroInDegree.empty())
    {
        std::string u = zeroInDegree.front();
        zeroInDegree.pop();
        topoOrder.push_back(u);

        for (const std::string& v : children[u])
        {
            inDegree[v]--;
            if (inDegree[v] == 0)
            {
                zeroInDegree.push(v);
            }
        }
    }

    // 4. Проверка на цикл
    if (topoOrder.size() != tasks.size())
    {
        m_logger.Error("Cycle detected during topological sort!");
        return {}; // пустой вектор = ошибка
    }

    // 5. Преобразуем обратно в ScAddr
    std::vector<ScAddr> result;
    for (const std::string& name : topoOrder)
    {
        result.push_back(nameToAddr[name]);
    }
    return result;
}
ScResult TopologicalSortAgent::DoProgram(ConnectivityEvent const & event, ScAction & action){
    m_logger.Info("TopologicalSortAgent::DoProgram started!");
    m_logger.Debug("TopologicalSortAgent::DoProgram started!");
    ScAddr project = event.GetArcTargetElement();
    std::string projId = m_context.GetElementSystemIdentifier(project);
    m_logger.Info("TopologicalSortAgent: project system id = ", projId);
    ScAddrVector tasks = GetTasks(project);
    m_logger.Info("TopologicalSortAgent: found tasks count = ", tasks.size());
    for (auto const & t : tasks)
    {
        std::string id = m_context.GetElementSystemIdentifier(t);
        m_logger.Info("  task: ", id);
    }
    if(IsAcyclic(tasks)){
         m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::concept_acyclic_project,
        project);
    }
    else{
        m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::concept_cyclic_project,
        project);
        return action.FinishUnsuccessfully();
    }
    std::vector<ScAddr> topoOrder = TopologicalSort(tasks);
    m_logger.Info("TopologicalSortAgent: topoOrder size = ", topoOrder.size());
    for (auto const & t : topoOrder)
    {
        std::string id = m_context.GetElementSystemIdentifier(t);
        m_logger.Info("  topo: ", id);
    }
    if (topoOrder.empty())
    {
        // Цикл — уже обработано ранее, но на всякий случай
        return action.FinishUnsuccessfully();
    }

    // Создаём кортеж результата
    ScAddr orderTuple = m_context.GenerateNode(ScType::ConstNodeTuple);

    // Добавляем каждую задачу в кортеж (common arcs from tuple -> task).
    // Note: sc-memory iteration over common arcs may return elements in
    // reverse insertion order, so insert in reverse to make iterator
    // return tasks in the expected topological sequence.
    for (size_t ii = 0; ii < topoOrder.size(); ++ii)
    {
        size_t i = topoOrder.size() - 1 - ii;
        m_context.GenerateConnector(ScType::ConstCommonArc, orderTuple, topoOrder[i]);
    }

    // Создаём общую дугу от project -> orderTuple и помечаем эту дугу nrel_topological_order
    ScAddr projectToTupleArc = m_context.GenerateConnector(ScType::ConstCommonArc, project, orderTuple);
    m_logger.Info("TopologicalSortAgent: created project->tuple arc");
    m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_topological_order, projectToTupleArc);
    m_logger.Info("TopologicalSortAgent: marked project->tuple arc with nrel_topological_order");

    return action.FinishSuccessfully();
    
}

