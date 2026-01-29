let indexes = null

const toggle = document.getElementById("togglePanel");
const overlay = document.getElementById("overlay");
const panel_content = document.getElementById("panel-content");

const prev_button = document.getElementById("prevMonth");
const next_button = document.getElementById("nextMonth");

const date_text = document.getElementById("month-title");

document.getElementById("menu-button").addEventListener("click", () => {
    window.location.replace("/");
});

function prevMonth(year, month)
{
    if (month == 0) {
        return { year: year - 1, month: 11 };
    }
    return { year, month: month - 1 };
}

function nextMonth(year, month)
{
    if (month == 11) {
        return { year: year + 1, month: 0 };
    }
    return { year, month: month + 1 };
}

prev_button.addEventListener("click", () => {
    let prev_date = prevMonth(date_text.year, date_text.month);
    create_calender_at_date(prev_date['year'], prev_date['month']);
});

next_button.addEventListener("click", () => {
    let next_date = nextMonth(date_text.year, date_text.month);
    create_calender_at_date(next_date['year'], next_date['month']);
});

/*
 * 
 * 
 *  Event Listener setup
 * 
 * 
 */
overlay.addEventListener("click", closePanel);
toggle.addEventListener("click", () => {
    document.body.classList.contains("panel-open") ? closePanel() : openPanel();
});

/**
 * 
 * 
 *  Post Functions
 * 
 * 
 */

async function get_task_list(year, month, day)
{
    month += 1;
    const response = await fetch("task/get", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ year, month, day })
    });

    const contentType = response.headers.get("content-type");

    if (contentType && contentType.includes("application/json"))
    {
        const json = await response.json();
        return json;
    } else
    {
        const text = await response.text();
        return null;
    }
}

function normalizeYMD(ymd)
{
    // Accepts "YYYY-M-D" or "YYYY-MM-DD"
    const [y, m, d] = String(ymd).trim().split("-");
    if (!y || !m || !d) return ymd;
    return `${y}-${String(m).padStart(2, "0")}-${String(d).padStart(2, "0")}`;
}

async function set_task_status(task_id, status, btn)
{
    const response = await fetch("task/status", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            task_id,
            status,
            year: Number(btn.dataset.year),
            month: Number(btn.dataset.month),
            day: Number(btn.dataset.day)
        })
    });

    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to update status: ${response.status} ${text}`);
        return false;
    }

    return true;
}

async function save_new_task(title_inp, desc_inp, time_due_inp, date_due_inp, sort_order_inp)
{
    let title = title_inp.value;
    let description = desc_inp.value;
    let time_due = time_due_inp.value;
    let date_due = date_due_inp ? date_due_inp.value : selectedDate.textContent;
    let sort_order = sort_order_inp.value;

    const response = await fetch("task/add", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ title, description, time_due, date_due, sort_order })
    });

    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to add task: ${response.status} ${text}`);
    }

    const json = await response.json();
    const tasks = extract_tasks(json);
    const btn = document.querySelector(`.day[data-date="${normalizeYMD(date_due)}"]`);
    console.log(normalizeYMD(date_due));
    btn._tasks = tasks;
    render_day_tasks(btn);
    closePanel();
    return;
}

function remove_task_from_btn(btn, task_id)
{
    if (!Array.isArray(btn._tasks)) return;

    btn._tasks = btn._tasks.filter(t => Number(t.task_id) !== Number(task_id));
}

async function save_edit_task(task_id, current_set_date, title_inp, desc_inp, time_due_inp, date_due_inp, sort_order_inp)
{
    let title = title_inp.value;
    let description = desc_inp.value;
    let time_due = time_due_inp.value;
    let date_due = date_due_inp ? date_due_inp.value : selectedDate.textContent;
    let sort_order = sort_order_inp.value;

    let response;
    if (current_set_date)
    {
        response = await fetch("task/add", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({task_id, current_set_date, title, description, time_due, date_due, sort_order })
        });
    }else
    {
        response = await fetch("task/add", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({task_id, title, description, time_due, date_due, sort_order })
        });
    }
    

    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to add task: ${response.status} ${text}`);
    }

    const json = await response.json();
    const tasks = extract_tasks(json);
    const btn = document.querySelector(`.day[data-date="${normalizeYMD(date_due)}"]`);
    btn._tasks = tasks;
    render_day_tasks(btn);

    const old_btn = document.querySelector(`.day[data-date="${normalizeYMD(current_set_date)}"]`);
    remove_task_from_btn(old_btn, task_id);
    render_day_tasks(old_btn);
    closePanel();
    return;
}

async function delete_task(task_id, btn)
{
    const response = await fetch("task/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            task_id,
            year: Number(btn.dataset.year),
            month: Number(btn.dataset.month),
            day: Number(btn.dataset.day)
        })
    });

    if (!response.ok)
    {
        const text = await response.text().catch(() => "");
        throw new Error(`Failed to update status: ${response.status} ${text}`);
        return false;
    }

    return true;
}

/**
 * 
 * 
 *  Calender Functions
 * 
 * 
 */

async function create_calendar(year, month)
{
    const calendar = document.getElementById("calendar-grid");

    calendar.querySelectorAll(".day, .empty").forEach(e => e.remove());

    const firstDay = new Date(year, month, 1).getDay();
    const daysInMonth = new Date(year, month + 1, 0).getDate();

    /* Add empty days until the first day of the month */ 
    for (let i = 0; i < firstDay; i++)
    {
        const empty = document.createElement("div");
        empty.className = "empty";
        calendar.appendChild(empty);
    }

    for (let day = 1; day <= daysInMonth; day++)
    {
        let task_list = await get_task_list(year, month, day);
        const btn = document.createElement("button");
        btn.className = "day";

        const ymd = `${year}-${String(month + 1).padStart(2,"0")}-${String(day).padStart(2,"0")}`;
        btn.dataset.date = ymd;
        btn.dataset.year = year;
        btn.dataset.month = month + 1;
        btn.dataset.day = day;

        const num = document.createElement("span");
        num.className = "num";
        num.textContent = day;

        btn.appendChild(num);
        let all_tasks;
        btn._tasks = null;

        if (task_list)
        {
            all_tasks = extract_tasks(task_list);
            btn._tasks = all_tasks;
            render_day_tasks(btn);
        }

        calendar.appendChild(btn);
        btn.addEventListener("click", () => {
            set_panel_to_display_tasks(btn);
            openPanel();
        });
    }

    set_month_title(year, month);
}

let disabledGroupIds = new Set();

function isTaskVisible(task, indexes)
{
    const groups = indexes?.taskToGroups.get(Number(task.task_id)) ?? [];
    if (groups.length === 0) return true;

    return !groups.some(g => disabledGroupIds.has(Number(g.group_id)));
}

function render_day_tasks(btn, maxVisible = 4)
{
    const tasks = btn._tasks;

    btn.querySelectorAll(".day-tasks").forEach(n => n.remove());

    if (!tasks || tasks.length == 0) return;

    const incomplete = tasks.filter(t => t.status != 1);
    if (incomplete.length == 0) return;

    const visibleTasks = incomplete.filter(t => isTaskVisible(t, indexes));
    if (visibleTasks.length === 0) return;

    const wrap = document.createElement("div");
    wrap.className = "day-tasks";

    const visible = visibleTasks.slice(0, maxVisible);

    for (const t of visible)
    {
        const pill = document.createElement("div");
        pill.className = "task-pill";
        pill.textContent = t.name ?? "(untitled)";
        let colour = "#1f6feb";
        if (indexes)
            colour = get_task_colour(t, indexes);
        pill.style.backgroundColor = colour;
        wrap.appendChild(pill);
    }

    const remaining = incomplete.length - visible.length;
    if (remaining > 0) {
        const more = document.createElement("div");
        more.className = "task-more";
        more.textContent = `+${remaining} more`;
        wrap.appendChild(more);
    }

    btn.appendChild(wrap);
}

/**
 * 
 * 
 *  Utility and Helpers
 * 
 * 
 * 
 */

function set_month_title(year, month)
{
    const monthNames = [
        "January", "February", "March", "April",
        "May", "June", "July", "August",
        "September", "October", "November", "December"
    ];
    date_text.textContent = `${monthNames[month]} ${year}`;
    date_text.year = year;
    date_text.month = month;
}

function extract_tasks(json)
{
    const tasks = [];
    for (const [key, value] of Object.entries(json))
    {
        if (!value || typeof value !== "object") continue;
        if (!("task_id" in value) || !("time_due" in value)) continue;

        tasks.push(value);
    }
    tasks.sort((a, b) => {
        const so = (a.sort_order ?? 0) - (b.sort_order ?? 0);
        if (so !== 0) return so;
        return new Date(a.time_due.trim()) - new Date(b.time_due.trim());
    });
    return tasks;
}

/*
 * 
 * 
 *  Task Panel UI functions 
 * 
 * 
 */

function openPanel()
{
    document.body.classList.add("panel-open");
    toggle.textContent = "◂ Tasks";
}

function closePanel()
{
    document.body.classList.remove("panel-open");
    toggle.textContent = "Tasks ▸";
    panel_reset();
}

function panel_reset()
{
    panel_content.textContent = "";
    selectedDate.textContent = "None";
}

function set_panel_to_add_task()
{
    panel_content.textContent = "";

    let title_label = document.createElement("label");
    title_label.textContent = "Title";
    let title_input = document.createElement("input");
    title_input.type = "text";
    title_label.appendChild(title_input);
    panel_content.appendChild(title_label);

    let date_input = 0;
    if (selectedDate.textContent == "None")
    {
        let date_label = document.createElement("label");
        date_label.textContent = "Date Due";
        date_input = document.createElement("input");
        date_input.type = "date";
        date_label.appendChild(date_input);
        panel_content.appendChild(date_label);
    }

    let time_label = document.createElement("label");
    time_label.textContent = "Time Due";
    let time_input = document.createElement("input");
    time_input.type = "time";
    time_label.appendChild(time_input);
    panel_content.appendChild(time_label);

    let descr_label = document.createElement("label");
    descr_label.textContent = "Desription";
    let description_input = document.createElement("input");
    description_input.type = "text";
    descr_label.appendChild(description_input);
    panel_content.appendChild(descr_label);

    let sort_label = document.createElement("label");
    sort_label.textContent = "Sort Order";
    let sort_input = document.createElement("input");
    sort_input.type = "text";
    sort_input.inputMode = "numeric";
    sort_input.addEventListener("input", () => {
        sort_input.value = sort_input.value.replace(/\D/g, "");
    });
    sort_label.appendChild(sort_input);
    panel_content.appendChild(sort_label);

    let save_btn = document.createElement("button");
    save_btn.className = "save-btn";
    save_btn.textContent = "Save";
    save_btn.addEventListener("click", () => {
        save_new_task(title_input, description_input, time_input, date_input, sort_input);
    });
    panel_content.appendChild(save_btn);
}

function set_panel_to_edit_task(btn, task)
{
    panel_content.textContent = "";

    let title_label = document.createElement("label");
    title_label.textContent = "Title";
    let title_input = document.createElement("input");
    title_input.value = task.name;
    title_input.type = "text";
    title_label.appendChild(title_input);
    panel_content.appendChild(title_label);

    let date_label = document.createElement("label");
    date_label.textContent = "Date Due";
    let date_input = document.createElement("input");
    date_input.type = "date";
    date_input.value = btn.dataset.date;
    date_label.appendChild(date_input);
    panel_content.appendChild(date_label);

    let time_label = document.createElement("label");
    time_label.textContent = "Time Due";
    let time_input = document.createElement("input");
    time_input.type = "time";
    const d = new Date(task.time_due);
    const hh = String(d.getHours()).padStart(2, "0");
    const mm = String(d.getMinutes()).padStart(2, "0");
    time_input.value = `${hh}:${mm}`;
    time_label.appendChild(time_input);
    panel_content.appendChild(time_label);

    let descr_label = document.createElement("label");
    descr_label.textContent = "Desription";
    let description_input = document.createElement("input");
    description_input.type = "text";
    description_input.value = task.description;
    descr_label.appendChild(description_input);
    panel_content.appendChild(descr_label);

    let sort_label = document.createElement("label");
    sort_label.textContent = "Sort Order";
    let sort_input = document.createElement("input");
    sort_input.type = "text";
    sort_input.value = task.sort_order;
    sort_input.inputMode = "numeric";
    sort_input.addEventListener("input", () => {
        sort_input.value = sort_input.value.replace(/\D/g, "");
    });
    sort_label.appendChild(sort_input);
    panel_content.appendChild(sort_label);

    let save_btn = document.createElement("button");
    save_btn.className = "save-btn";
    save_btn.textContent = "Save";
    save_btn.addEventListener("click", () => {
        if (btn.dataset.date != date_input.value)
            save_edit_task(task.task_id, btn.dataset.date, title_input, description_input, time_input, date_input, sort_input);
        else
            save_edit_task(task.task_id, null, title_input, description_input, time_input, date_input, sort_input);
    });
    panel_content.appendChild(save_btn);
}
async function set_panel_to_display_tasks(btn)
{
    selectedDate.textContent = `${btn.dataset.year}-${btn.dataset.month}-${btn.dataset.day}`;
    if (btn._tasks)
    {
        for (let t of btn._tasks)
        {
            let label = document.createElement("div");
            label.className = "assigned_task";

            let title_div = document.createElement("div");
            title_div.className = "title_div";
            if (indexes)
                 title_div.style.backgroundColor = get_task_colour(t, indexes);

            let check_box = document.createElement("input");
            check_box.type = "checkbox";
            check_box.checked = (t.status == 1);
            title_div.appendChild(check_box);

            let title = document.createElement("p");
            title.className = "assigned_task_title"
            title.textContent =  `${t.name}`;
            title_div.appendChild(title);

            label.appendChild(title_div);

            let description = document.createElement("p");
            description.className = "assigned_task_p";
            description.textContent =  `${t.description}`;
            label.appendChild(description);

            let time_due = document.createElement("p");
            time_due.className = "assigned_task_p";
            time_due.textContent =  `Due: ${t.time_due}`;
            label.appendChild(time_due);

            let edit_div = document.createElement("div");
            edit_div.className = "edit_div";
            let edit_p = document.createElement("p");
            edit_p.textContent = "edit";
            edit_p.className = "little_link";
            edit_p.addEventListener("click", () => {
                set_panel_to_edit_task(btn, t);
            });

            let delete_p = document.createElement("p");
            delete_p.textContent = "delete";
            delete_p.className = "little_link";
            delete_p.addEventListener("click", async () => {
                if (await delete_task(t.task_id, btn) == true)
                {
                    closePanel();
                    btn._tasks = btn._tasks.filter(x => x !== t);
                    render_day_tasks(btn);
                }
            });
            edit_div.appendChild(edit_p);
            edit_div.appendChild(delete_p);
            label.appendChild(edit_div);

            panel_content.appendChild(label);

            check_box.addEventListener("change", async () => {
                const newStatus = check_box.checked ? 1 : 0;

                try {
                    if (await set_task_status(t.task_id, newStatus, btn) == 1)
                    {
                        t.status = newStatus;
                        render_day_tasks(btn);
                    }
                } catch (err) {
                    console.error(err);
                    check_box.checked = !check_box.checked;
                }
            });
        }
    }
}

function get_task_colour(task, indexes)
{
    const groups = indexes.taskToGroups.get(Number(task.task_id));
    if (!groups || groups.length === 0) return null;

    return intToHexColor(groups[0].colour);
}

async function initialize_calender()
{
    const now = new Date();
    let currentYear = now.getFullYear();
    let currentMonth = now.getMonth();

    let groups = await load_groups();
    indexes = build_group_indexes(groups);

    create_calendar(currentYear, currentMonth);
}

async function create_calender_at_date(year, month)
{
    let groups = await load_groups();
    indexes = build_group_indexes(groups);

    create_calendar(year, month);
}