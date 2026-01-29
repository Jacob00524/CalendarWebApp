const group_btn = document.getElementById("group-button");
const group_panel = document.getElementById("group-panel");
const close_groups = document.getElementById("closeGroups");

const group_name_inp = document.getElementById("groupName");
const colour_name_inp = document.getElementById("groupColor");
const task_search_inp = document.getElementById("availSearch");
const group_search_inp = document.getElementById("groupSearch");

const task_list_panel = document.getElementById("availList");
const group_list_panel = document.getElementById("groupList");

const addBtn = document.getElementById("addToGroup");
const removeBtn = document.getElementById("removeFromGroup");
const saveBtn = document.getElementById("saveGroup");

const group_header = document.getElementById("group-header-content");

let selectedAvail = null;
let selectedGroup = null;

let groupSet = null;
let group_id = 0;

let group_div_ = null;

let groups = null;

addBtn.addEventListener("click", () => {
    for (const id of selectedAvail) groupSet.add(id);
    selectedAvail.clear();
    render(collect_all_tasks());
});

removeBtn.addEventListener("click", () => {
    for (const id of selectedGroup) groupSet.delete(id);
    selectedGroup.clear();
    render(collect_all_tasks());
});

saveBtn.addEventListener("click", async () => {
    if (!group_name_inp.value)
    {
        alert("group name can't be empty.");
        return;
    }
    await save_group(groupSet, group_name_inp.value, colour_name_inp.value);
    indexes = build_group_indexes(groups);
    close_group_panel();
    group_div_.querySelector("#group_name_p").textContent = group_name_inp.value;
    group_div_.style.backgroundColor = colour_name_inp.value;

    const dayButtons = document.querySelectorAll(".day");
    for (const btn of dayButtons)
    {
        render_day_tasks(btn);
    }
    render_groups(groups);
});

function intToHexColor(int)
{
  const hex = (int & 0x00FFFFFF)
    .toString(16)
    .toUpperCase();
  return "#" + hex.padStart(6, "0");
}

async function save_group(groupSet, name, colour)
{
    const response = await fetch("group/edit", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            task_ids: [...groupSet],
            name,
            colour,
            group_id
        })
    });
    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to save group: ${response.status} ${text}`);
    }
    groups = await response.json();

    return true;
}

group_btn.addEventListener("click", () => {
    reset_group_panel();
    init_new_group_panel();
    open_group_panel();
});

close_groups.addEventListener("click", () => {
    reset_group_panel();
    close_group_panel();
});

function open_group_panel()
{
    group_panel.style.display = "flex";
}

function close_group_panel()
{
    group_panel.style.display = "none";
}

function reset_group_panel()
{
    group_name_inp.value = "";
    task_search_inp.value = "";
    group_search_inp.value = "";

    task_list_panel.innerText = "";
    group_list_panel.innerText = "";

    selectedAvail = new Set();
    selectedGroup = new Set();
    groupSet = new Set();

    document.getElementById("availCount").textContent = "0";
    document.getElementById("groupCount").textContent = "0";
}

function getAvailableTasks(allTasks) {
    return allTasks.filter(t => !groupSet.has(Number(t.task_id)));
}

function getGroupTasks(allTasks) {
    return allTasks.filter(t => groupSet.has(Number(t.task_id)));
}

function collect_all_tasks()
{
    const allTasks = new Map();

    document.querySelectorAll(".day").forEach(btn => {
        if (!Array.isArray(btn._tasks)) return;

        for (const t of btn._tasks) {
            if (!t || t.task_id == null) continue;
            allTasks.set(t.task_id, t);
        }
    });

    return [...allTasks.values()];
}

function makeRow(task, pane)
{
    const id = Number(task.task_id);

    const row = document.createElement("div");
    row.className = "task-row";

    const cb = document.createElement("input");
    cb.type = "checkbox";

    const isChecked = (pane === "avail")
        ? selectedAvail.has(id)
        : selectedGroup.has(id);

    cb.checked = isChecked;

    // Only checkbox click toggles selection (row click doesn't)
    cb.addEventListener("click", (e) => {
        e.stopPropagation();
        const set = pane === "avail" ? selectedAvail : selectedGroup;
        if (cb.checked) set.add(id);
        else set.delete(id);
        updateButtons();
    });

    const main = document.createElement("div");
    main.className = "task-main";

    const title = document.createElement("div");
    title.className = "task-title";
    title.textContent = task.name ? task.name : "(untitled)";

    const sub = document.createElement("div");
    sub.className = "task-sub";
    sub.textContent = task.description ?? "";

    main.appendChild(title);
    if (sub.textContent) main.appendChild(sub);

    const pill = document.createElement("div");
    pill.className = "task-pill";
    pill.textContent = (task.time_due ? `Due: ${task.time_due}` : "");

    row.appendChild(cb);
    row.appendChild(main);
    row.appendChild(pill);

    return row;
}

function matchesSearch(task, q)
{
    if (!q) return true;
    const hay = `${task.name ?? ""} ${task.description ?? ""} ${task.time_due ?? ""}`;
    return normalize(hay).includes(normalize(q));
}

function render(allTasks) {
    const availQ = availSearch.value;
    const groupQ = groupSearch.value;

    const availTasks = getAvailableTasks(allTasks).filter(t => matchesSearch(t, availQ));
    const groupTasks = getGroupTasks(allTasks).filter(t => matchesSearch(t, groupQ));

    // Clean up stale selections (if tasks moved)
    for (const id of [...selectedAvail]) {
        if (groupSet.has(id)) selectedAvail.delete(id);
    }
    for (const id of [...selectedGroup]) {
        if (!groupSet.has(id)) selectedGroup.delete(id);
    }

    availList.innerHTML = "";
    groupList.innerHTML = "";

    for (const t of availTasks) availList.appendChild(makeRow(t, "avail"));
    for (const t of groupTasks) groupList.appendChild(makeRow(t, "group"));

    availCount.textContent = `${availTasks.length} shown`;
    groupCount.textContent = `${groupTasks.length} shown`;

    updateButtons();
}

function updateButtons()
{
    addBtn.disabled = selectedAvail.size == 0;
    removeBtn.disabled = selectedGroup.size == 0;
}

function randInt()
{
    const UINT_MAX = 0xFFFFFFFF;
    const n = Math.floor(Math.random() * (UINT_MAX + 1));
    return n;
}

function init_new_group_panel()
{
    const all_tasks = collect_all_tasks();
    group_id = 0;
    document.getElementById("availCount").textContent = `${all_tasks.length}`;

    for (const t of all_tasks)
    {
        let row = makeRow(t, "avail");
        task_list_panel.appendChild(row);
    }
    updateButtons();
}
function init_group_panel(group_json)
{
    const all_tasks = collect_all_tasks();

    group_id = group_json.group_id;
    for (let i = 0; i < group_json.ids.length; i++)
        groupSet.add(group_json.ids[i]);
    for (let t = 0; t < all_tasks.length; t++)
    {
        let id_match = false;
        for (let i = 0; i < group_json.ids.length; i++)
        {
            if (all_tasks[t].task_id == group_json.ids[i])
            {
                let group_row = makeRow(all_tasks[t], "group");
                group_list_panel.appendChild(group_row);
                id_match = true;
                break;
            }
        }
        if (id_match == false)
        {
            let avail_row = makeRow(all_tasks[t], "avail");
            task_list_panel.appendChild(avail_row);
        }
    }
    document.getElementById("availCount").textContent = `${all_tasks.length - group_json.ids.length}`;
    document.getElementById("groupCount").textContent = `${group_json.ids.length} shown`;
    group_name_inp.value = group_json.name;
    colour_name_inp.value = intToHexColor(group_json.colour);
    updateButtons();
}

function build_group_indexes(groups_)
{
    const groupToTaskIds = new Map();
    const taskToGroups   = new Map();

    groups = groups_;

    for (const g of groups)
    {
        const gid = Number(g.group_id);
        const ids = new Set((g.ids ?? []).map(Number));
        groupToTaskIds.set(gid, ids);

        for (const tid of ids)
        {
            if (!taskToGroups.has(tid)) taskToGroups.set(tid, []);
            taskToGroups.get(tid).push({
                group_id: gid,
                name: g.name,
                colour: g.colour
            });
        }
    }

    return { groupToTaskIds, taskToGroups };
}

function render_groups(group_data)
{
    group_header.textContent = "";
    /* loop through all groups and render them */
    for (let g of group_data)
    {
        let group_div = document.createElement("div");
        group_div.className = "group-select-div";

        let group_check = document.createElement("input");
        group_check.type = "checkbox";
        group_check.dataset.groupId = g.group_id;
        if (disabledGroupIds.has(g.group_id))
            group_check.checked = false;
        else
            group_check.checked = true;
        group_check.addEventListener("click", (e) => {
            e.stopPropagation();
            const gid = Number(group_check.dataset.groupId);

            if (group_check.checked) disabledGroupIds.delete(gid);
            else disabledGroupIds.add(gid);

            for (const btn of document.querySelectorAll(".day"))
                render_day_tasks(btn);
        });

        let group_name = document.createElement("p");
        group_name.textContent = g.name;
        group_name.id = "group_name_p";

        group_div.appendChild(group_check);
        group_div.appendChild(group_name);
        group_div.style.backgroundColor = intToHexColor(g.colour);
        group_header.appendChild(group_div);

        group_div.json_data = g;
        group_div.addEventListener("click", () => {
            reset_group_panel();
            group_div_ = group_div;
            init_group_panel(group_div.json_data);
            open_group_panel();
        });
    }
}

async function load_groups()
{
    const response = await fetch("group/fetch", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        }
    });

    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to save group: ${response.status} ${text}`);
    }

    const groups_json = await response.json();
    render_groups(groups_json);
    
    return groups_json;
}

