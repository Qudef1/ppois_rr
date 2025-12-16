#include "topological_sort_agent.hpp"
#include <sc-memory/sc_memory.hpp>
#include "../keynodes/scheduling_keynodes.hpp"
#include <functional>

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

// Получение main_idtf задачи
std::string TopologicalSortAgent::GetTaskMainIdtf(ScAddr const & task)
{
    ScIterator5Ptr it5 = m_context.CreateIterator5(
        task,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        ScKeynodes::nrel_main_idtf);
    
    if (it5->Next())
    {
        std::string content;
        m_context.GetLinkContent(it5->Get(2), content);
        return content;
    }
    
    // Если main_idtf нет, используем system identifier как fallback
    return m_context.GetElementSystemIdentifier(task);
}

// Сохранение минимальной продолжительности проекта
void TopologicalSortAgent::SaveProjectDuration(ScAddr const & project, uint32_t duration)
{
    WriteAttr(project, ProjectSchedulingKeynodes::nrel_duration, duration);
    m_logger.Info("Project duration saved: ", duration);
}

// Построение критического пути как последовательности задач
ScAddrVector TopologicalSortAgent::BuildCriticalPath(ScAddrVector const & topoOrder, std::map<std::string, uint32_t> const & slack)
{
    ScAddrVector criticalPath;
    
    // Строим граф зависимостей для критических задач
    // Используем system identifier как ключ (так как slack использует system identifier)
    std::map<std::string, ScAddr> nameToAddr;
    std::map<std::string, std::vector<std::string>> criticalChildren; // только для критических задач
    
    // Собираем критические задачи и их адреса
    for (ScAddr const & task : topoOrder)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (name.empty()) continue;
        if (slack.count(name) && slack.at(name) == 0)
        {
            nameToAddr[name] = task;
            criticalChildren[name] = std::vector<std::string>();
        }
    }
    
    // Строим граф зависимостей только между критическими задачами
    for (ScAddr const & task : topoOrder)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (name.empty() || !criticalChildren.count(name)) continue;
        
        auto children = GetChildren(task);
        for (ScAddr const & child : children)
        {
            std::string childName = m_context.GetElementSystemIdentifier(child);
            if (!childName.empty() && criticalChildren.count(childName))
            {
                criticalChildren[name].push_back(childName);
            }
        }
    }
    
    // Находим начальные критические задачи (те, у которых нет критических родителей)
    std::vector<std::string> startTasks;
    for (auto const & [name, addr] : nameToAddr)
    {
        bool hasCriticalParent = false;
        ScAddr task = addr;
        auto parents = GetDependencies(task);
        
        for (ScAddr const & parent : parents)
        {
            std::string parentName = m_context.GetElementSystemIdentifier(parent);
            if (!parentName.empty() && criticalChildren.count(parentName))
            {
                hasCriticalParent = true;
                break;
            }
        }
        
        if (!hasCriticalParent)
        {
            startTasks.push_back(name);
        }
    }
    
    // Если есть критические задачи, строим путь
    if (!startTasks.empty())
    {
        // Собираем длительности критических задач
        std::map<std::string, uint32_t> criticalDurations;
        for (auto const & [name, addr] : nameToAddr)
        {
            criticalDurations[name] = GetDuration(addr);
        }
        
        // Используем DFS для построения пути с максимальной суммарной длительностью через критические задачи
        std::function<std::pair<std::vector<std::string>, uint32_t>(const std::string&)> dfsLongestPath = 
            [&](const std::string& current) -> std::pair<std::vector<std::string>, uint32_t>
        {
            std::vector<std::string> bestPath = {current};
            uint32_t bestDuration = criticalDurations[current];
            
            if (criticalChildren.count(current))
            {
                for (const std::string& child : criticalChildren.at(current))
                {
                    auto [path, pathDuration] = dfsLongestPath(child);
                    uint32_t totalDuration = criticalDurations[current] + pathDuration;
                    if (totalDuration > bestDuration)
                    {
                        bestPath = {current};
                        bestPath.insert(bestPath.end(), path.begin(), path.end());
                        bestDuration = totalDuration;
                    }
                }
            }
            
            return {bestPath, bestDuration};
        };
        
        // Находим путь с максимальной длительностью из всех начальных задач
        std::vector<std::string> longestPath;
        uint32_t maxDuration = 0;
        for (const std::string& start : startTasks)
        {
            auto [path, pathDuration] = dfsLongestPath(start);
            if (pathDuration > maxDuration)
            {
                longestPath = path;
                maxDuration = pathDuration;
            }
        }
        
        // Преобразуем имена в ScAddr
        for (const std::string& name : longestPath)
        {
            if (nameToAddr.count(name))
            {
                criticalPath.push_back(nameToAddr.at(name));
            }
        }
    }
    
    m_logger.Info("Critical path built with ", criticalPath.size(), " tasks");
    return criticalPath;
}

// Выявление задач, которые могут выполняться параллельно
void TopologicalSortAgent::FindParallelTasks(ScAddrVector const & topoOrder, std::map<std::string, uint32_t> const & ES, std::map<std::string, uint32_t> const & EF)
{
    std::map<std::string, ScAddr> nameToAddr;
    for (ScAddr const & task : topoOrder)
    {
        std::string name = m_context.GetElementSystemIdentifier(task);
        if (!name.empty())
            nameToAddr[name] = task;
    }
    
    // Группируем задачи по интервалам времени [ES, EF]
    // Задачи могут выполняться параллельно, если их интервалы перекрываются
    // и они не связаны зависимостью
    
    std::vector<std::pair<std::string, ScAddr>> parallelGroups;
    
    for (size_t i = 0; i < topoOrder.size(); ++i)
    {
        std::string name1 = m_context.GetElementSystemIdentifier(topoOrder[i]);
        if (name1.empty() || !ES.count(name1) || !EF.count(name1)) continue;
        
        uint32_t es1 = ES.at(name1);
        uint32_t ef1 = EF.at(name1);
        
        for (size_t j = i + 1; j < topoOrder.size(); ++j)
        {
            std::string name2 = m_context.GetElementSystemIdentifier(topoOrder[j]);
            if (name2.empty() || !ES.count(name2) || !EF.count(name2)) continue;
            
            uint32_t es2 = ES.at(name2);
            uint32_t ef2 = EF.at(name2);
            
            // Проверяем, перекрываются ли интервалы
            bool intervalsOverlap = !(ef1 <= es2 || ef2 <= es1);
            
            if (intervalsOverlap)
            {
                // Проверяем, что нет прямой зависимости
                ScAddr task1 = nameToAddr[name1];
                ScAddr task2 = nameToAddr[name2];
                
                auto deps1 = GetDependencies(task1);
                auto deps2 = GetDependencies(task2);
                
                bool hasDirectDependency = false;
                for (ScAddr const & dep : deps1)
                {
                    if (dep == task2)
                    {
                        hasDirectDependency = true;
                        break;
                    }
                }
                for (ScAddr const & dep : deps2)
                {
                    if (dep == task1)
                    {
                        hasDirectDependency = true;
                        break;
                    }
                }
                
                if (!hasDirectDependency)
                {
                    // Эти задачи могут выполняться параллельно
                    // В реальной системе можно создать группу параллельных задач
                    m_logger.Info("Parallel tasks found: ", name1, " [", es1, "-", ef1, "] and ", name2, " [", es2, "-", ef2, "]");
                }
            }
        }
    }
}

// Построение зависимостей и вычисление CPM (ES, EF, LS, LF, Slack)
void TopologicalSortAgent::BuildTaskDependencies(ScAddrVector const & topoOrder, ScAddr const & project, std::map<std::string, uint32_t> & slackMap, uint32_t & projectDuration)
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
    projectDuration = 0;
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
    slackMap.clear();
    for (auto const & p : nameToAddr)
    {
        const std::string & name = p.first;
        ScAddr const & task = p.second;

        uint32_t es = ES[name];
        uint32_t ef = EF[name];
        uint32_t ls = LS[name];
        uint32_t lf = LF[name];
        uint32_t slack = (ls >= es) ? (ls - es) : 0;

        slackMap[name] = slack;

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

    // Сохраняем минимальную продолжительность проекта
    SaveProjectDuration(project, projectDuration);

    m_logger.Info("CPM attributes (ES, EF, LS, LF, Slack) successfully written.");
    
    // Выявляем параллельные задачи (вызывается после записи атрибутов)
    FindParallelTasks(topoOrder, ES, EF);
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
    std::map<std::string, uint32_t> slackMap;
    uint32_t projectDuration;
    BuildTaskDependencies(topoOrder, project, slackMap, projectDuration);

    // Строим критический путь и сохраняем его
    ScAddrVector criticalPath = BuildCriticalPath(topoOrder, slackMap);
    if (!criticalPath.empty())
    {
        ScAddr criticalPathTuple = m_context.GenerateNode(ScType::ConstNodeTuple);
        for (ScAddr const & task : criticalPath)
        {
            m_context.GenerateConnector(ScType::ConstCommonArc, criticalPathTuple, task);
        }
        ScAddr projectToCriticalPathArc = m_context.GenerateConnector(ScType::ConstCommonArc, project, criticalPathTuple);
        m_context.GenerateConnector(ScType::ConstPermPosArc, ProjectSchedulingKeynodes::nrel_critical_path, projectToCriticalPathArc);
        m_logger.Info("Critical path saved with ", criticalPath.size(), " tasks");
    }

    return action.FinishSuccessfully();
}
