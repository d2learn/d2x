import("core.base.json")
import("core.base.option")

local function xmake_program()
    return os.programfile() or "xmake"
end

local function get_d2x_lang()
    local d2x_json_file = path.join(os.projectdir(), ".d2x.json")
    if os.isfile(d2x_json_file) then
        local d2x_config = json.loadfile(d2x_json_file) or {}
        return d2x_config["lang"]
    end
    return nil
end

local function apply_project_config()
    local lang = get_d2x_lang()
    if lang and #lang > 0 then
        os.execv(xmake_program(), {"f", "--lang=" .. lang})
    end
end

local function print_target_files(name)
    local target_output = try {function ()
        return os.iorunv(xmake_program(), {"show", "-t", name, "--json"})
    end}
    if not target_output or #target_output == 0 then
        return
    end
    local target_info = try {function ()
        return json.decode(target_output)
    end}
    local flag = true

    printf(name)
    local files = (target_info and target_info.files) or {}
    for _, fileinfo in ipairs(files) do
        local file = fileinfo.path or fileinfo
        file = path.absolute(file)
        if flag then
            printf("@ " .. file)
            flag = false
        else
            printf(", " .. file)
        end
    end
    printf("\n")
end

function list(target)
    if target and #target > 0 then
        print_target_files(target)
        return
    end

    -- if target is nil, list all targets
    local target_names_output = try {function ()
        return os.iorunv(xmake_program(), {"show", "-l", "targets", "--json"})
    end}
    if not target_names_output or #target_names_output == 0 then
        return
    end
    local target_names = try {function ()
        return json.decode(target_names_output)
    end}
    if not target_names or #target_names == 0 then
        return
    end
    for _, name in ipairs(target_names) do
        print_target_files(name)
    end
end

function main()

    local command = option.get("command")
    local target = option.get("target")

    os.cd(os.projectdir())
    --print("project file: " .. project.rootfile())

    if command == "init" then
        apply_project_config()
    elseif command == "list" then
        list(target)
    elseif command == "build" then
        os.execv(xmake_program(), {"build", target or ""})
    elseif command == "run" then
        os.execv(xmake_program(), {"run", target or ""})
    else
        print("Unknown command: " .. tostring(command))
    end

end