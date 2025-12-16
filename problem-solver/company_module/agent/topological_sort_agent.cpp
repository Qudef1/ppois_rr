#include "topological_sort_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../keynodes/scheduling_keynodes.hpp"

// Конструктор агента, логируем инициализацию
TopologicalSortAgent::TopologicalSortAgent(){
  m_logger.Info("Topological sort agent initialized.");
}

// Подписка на событие: когда проект DAG готов
ScAddr TopologicalSortAgent::GetEventSubscriptionElement() const{
  return ProjectSchedulingKeynodes::action_project_dag_ready;
}

// Класс действия для TopologicalSort
ScAddr TopologicalSortAgent::GetActionClass() const{
  return ProjectSchedulingKeynodes::action_topological_check_dag;
}

// Шаблон для инициирования действия на основе события
ScTemplate TopologicalSortAgent::GetInitiationConditionTemplate(ConnectivityEvent const & event) const
{
  ScTemplate templ;
  templ.Triple(ProjectSchedulingKeynodes::action_project_dag_ready, ScType::VarPermPosArc, ScType::VarNode);
  return templ;
}

// Получение всех задач проекта
ScAddrVector TopologicalSortAgent::GetTasks(ScAddr const & project){
    ScAddrVector tasks;

    // Пытаемся собрать задачи через обычные дуги
    ScIterator3Ptr it3_any = m_context.CreateIterator3(project, ScType::ConstCommonArc, ScType::Unknown);
    while (it3_any->Next())
    {
        ScAddr const & elementAddr = it3_any->Get(2);
        tasks.push_back(elementAddr);
    }

    // Если ничего не нашли, пробуем через perm pos arcs
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

// Получение зависимостей конкретной задачи
ScAddrVector TopologicalSortAgent::GetDependencies(ScAddr const & task){
    ScAddrVector dependencies;
    // Ищем common arcs от child -> parent, которые помечены nrel_dependency
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        task,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::nrel_dependency);

    while (it5->Next())
    {
        ScAddr parent = it5->Get(2); // target common arc — родитель
        dependencies.push_back(parent);
    }
    return dependencies;
}

// Проверка, что DAG ацикличен (DFS по ScAddr)
bool TopologicalSortAgent::IsAcyclic(ScAddrVector const & tasks)
{
    // Сопоставляем ScAddr с именами
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

    // Инициализация цветов для DFS: 0=white,1=gray,2=black
    std::map<std::string, int> color;
    for (const std::string& name : taskNames)
        color[name] = 0;

    // DFS по каждой вершине
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
                color[u] = 1; // серый
                ScAddr uAddr = nameToAddr[u];
                std::vector<ScAddr> deps = GetDependencies(uAddr);

                for (const ScAddr& vAddr : deps)
                {
                    std::string v = m_context.GetElementSystemIdentifier(vAddr);
                    if (v.empty()) continue;
                    if (taskNames.find(v) == taskNames.end()) continue;

                    if (color[v] == 0)
                        stack.push(v);
                    else if (color[v] == 1)
                        return false; // найден цикл
                }
            }
            else if (color[u] == 1)
            {
                color[u] = 2; // черный
                stack.pop();
            }
            else
                stack.pop();
        }
    }

    return true;
}

// Топологическая сортировка Kahn
ScAddrVector TopologicalSortAgent::TopologicalSort(ScAddrVector const & tasks){
    std::map<std::string, ScAddr> nameToAddr;
    std::map<std::string, std::vector<std::string>> children; // parent_name -> [child_names]
    std::map<std::string, int> inDegree;

    for(const auto & task : tasks){
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (name.empty()) continue;

        nameToAddr[name] = task;
        inDegree[name] = 0;
        children[name] = std::vector<std::string>();
    }

    // Заполняем children и in-degree
    for (const ScAddr& childAddr : tasks)
    {
        std::string childName = m_context.GetElementSystemIdentifier(childAddr);
        if (childName.empty()) continue;

        auto parentAddrs = GetDependencies(childAddr);

        for (const ScAddr& parentAddr : parentAddrs)
        {
            std::string parentName = m_context.GetElementSystemIdentifier(parentAddr);
            if (parentName.empty()) continue;
            if (nameToAddr.find(parentName) == nameToAddr.end()) continue;

            children[parentName].push_back(childName);
            inDegree[childName]++;
        }
    }

    // Собираем задачи с нулевой in-degree
    std::queue<std::string> zeroInDegree;
    for (const auto& p : inDegree)
        if (p.second == 0)
            zeroInDegree.push(p.first);

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
                zeroInDegree.push(v);
        }
    }

    // Проверка на цикл
    if (topoOrder.size() != tasks.size())
    {
        m_logger.Error("Cycle detected during topological sort!");
        return {};
    }

    // Преобразуем обратно в ScAddr
    std::vector<ScAddr> result;
    for (const std::string& name : topoOrder)
        result.push_back(nameToAddr[name]);

    return result;
}

// Получение длительности задачи
uint32_t TopologicalSortAgent::GetDuration(ScAddr const & task){
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        task,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::nrel_duration
    );
    if (it5->Next())
    {
        std::string content;
        m_context.GetLinkContent(it5->Get(2), content);
        try
        {
            return std::stoul(content);
        }
        catch (...)
        {
            m_logger.Info("Invalid duration for task: ", m_context.GetElementSystemIdentifier(task));
        }
    }
    return 0;
}

// Получение детей (обратная зависимость)
ScAddrVector TopologicalSortAgent::GetChildren(ScAddr const & parent)
{
    ScAddrVector children;
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        ScType::ConstNode,
        ScType::ConstCommonArc,
        parent,
        ScType::ConstPermPosArc,
        ProjectSchedulingKeynodes::nrel_dependency);

    while (it5->Next())
        children.push_back(it5->Get(0));

    return children;
}

// Запись числового атрибута задачи в sc-память
void TopologicalSortAgent::WriteAttr(ScAddr const & task, ScAddr const & rel, uint32_t value)
{
    ScAddr link = m_context.GenerateLink(ScType::ConstNodeLink);
    m_context.SetLinkContent(link, std::to_string(value));

    ScAddr arc = m_context.GenerateConnector(ScType::ConstCommonArc, task, link);

    m_context.GenerateConnector(ScType::ConstPermPosArc, rel, arc);
}

// Построение зависимостей и вычисление CPM (ES, EF, LS, LF, Slack)
void TopologicalSortAgent::BuildTaskDependencies(ScAddrVector const & topoOrder)
{
    if (topoOrder.empty()) return;

    std::map<std::string, ScAddr> nameToAddr;
    std::map<std::string, uint32_t> ES, EF, LS, LF, duration;

    // Инициализация: собираем длительности
    for (ScAddr const & task : topoOrder)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (name.empty()) continue;
        nameToAddr[name] = task;
        duration[name] = GetDuration(task);
    }

    // Forward pass: ES и EF
    for (ScAddr const & task : topoOrder)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        uint32_t es = 0;

        auto parents = GetDependencies(task);
        for (ScAddr const & p : parents)
        {
            std::string pName = m_context.GetElementSystemIdentifier(p);
            if (pName.empty()) continue;
            if (EF.count(pName))
                es = std::max(es, EF[pName]);
        }

        ES[name] = es;
        EF[name] = es + duration[name];
    }

    // Определяем длительность проекта
    uint32_t projectDuration = 0;
    for (auto const & p : EF)
        projectDuration = std::max(projectDuration, p.second);

    // Backward pass: LS и LF
    for (auto it = topoOrder.rbegin(); it != topoOrder.rend(); ++it)
    {
        ScAddr task = *it;
        std::string name = m_context.GetElementSystemIdentifier(task);

        auto children = GetChildren(task);
        uint32_t lf = projectDuration;

        if (!children.empty())
        {
            lf = UINT32_MAX;
            for (ScAddr const & c : children)
            {
                std::string cName = m_context.GetElementSystemIdentifier(c);
                if (cName.empty()) continue;
                if (LS.count(cName))
                    lf = std::min(lf, LS[cName]);
            }
        }

        LF[name] = lf;
        LS[name] = lf - duration[name];
    }

    // Записываем атрибуты в sc-память
    for (auto const & p : nameToAddr)
    {
        const std::string & name = p.first;
        ScAddr const & task = p.second;

        uint32_t es = ES[name];
        uint32_t ef = EF[name];
        uint32_t ls = LS[name];
        uint32_t lf = LF[name];
        uint32_t slack = (ls >= es) ? (ls - es) : 0;

        WriteAttr(task, ProjectSchedulingKeynodes::nrel_es, es);
        WriteAttr(task, ProjectSchedulingKeynodes::nrel_ef, ef);
        WriteAttr(task, ProjectSchedulingKeynodes::nrel_ls, ls);
        WriteAttr(task, ProjectSchedulingKeynodes::nrel_lf, lf);
        WriteAttr(task, ProjectSchedulingKeynodes::nrel_slack, slack);

        if (slack == 0)
        {
            m_context.GenerateConnector(
                ScType::ConstPermPosArc,
                ProjectSchedulingKeynodes::concept_crytical_task,
                task);
        }
    }

    m_logger.Info("CPM attributes (ES, EF, LS, LF, Slack) successfully written.");
}

// Основная функция агента
ScResult TopologicalSortAgent::DoProgram(ConnectivityEvent const & event, ScAction & action){
    m_logger.Info("TopologicalSortAgent::DoProgram started!");
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

    // Проверка ацикличности
    if(IsAcyclic(tasks)){
        m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::concept_acyclic_project, project);
    }
    else{
        m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::concept_cyclic_project, project);
        return action.FinishUnsuccessfully();
    }

    // Топологическая сортировка
    std::vector<ScAddr> topoOrder = TopologicalSort(tasks);
    m_logger.Info("TopologicalSortAgent: topoOrder size = ", topoOrder.size());
    for (auto const & t : topoOrder)
        m_logger.Info("  topo: ", m_context.GetElementSystemIdentifier(t));

    if (topoOrder.empty())
        return action.FinishUnsuccessfully();

    // Создаём кортеж топологического порядка
    ScAddr orderTuple = m_context.GenerateNode(ScType::ConstNodeTuple);
    for (size_t ii = 0; ii < topoOrder.size(); ++ii)
    {
        size_t i = topoOrder.size() - 1 - ii;
        m_context.GenerateConnector(ScType::ConstCommonArc, orderTuple, topoOrder[i]);
    }

    // Связываем project -> orderTuple и помечаем nrel_topological_order
    ScAddr projectToTupleArc = m_context.GenerateConnector(ScType::ConstCommonArc, project, orderTuple);
    m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_topological_order, projectToTupleArc);

    // Вычисляем CPM атрибуты для задач
    BuildTaskDependencies(topoOrder);

    return action.FinishSuccessfully();
}
