-- Project-local LazyVim configuration for PRTN.
-- PRTN = PRTS Node
--
-- Board:
--   AirM2M CORE ESP32-C3
--
-- FQBN:
--   esp32:esp32:AirM2M_CORE_ESP32C3

if vim.lsp.log and vim.lsp.log.set_level then
	vim.lsp.log.set_level(vim.log.levels.ERROR)
else
	vim.lsp.set_log_level("ERROR")
end

local fqbn = "esp32:esp32:AirM2M_CORE_ESP32C3"
local clangd = vim.fn.exepath("clangd")
local arduino_cli = vim.fn.exepath("arduino-cli")
local arduino_ls = vim.fn.exepath("arduino-language-server")
local esp_rv32_query_driver = vim.fn.expand("~/.arduino15/packages/esp32/tools/esp-rv32/*/bin/riscv32-esp-elf-*")
local project_root = vim.fs.root(vim.fn.getcwd(), { ".lazy.lua", "PRTN.ino", "compile_commands.json" }) or vim.fn.getcwd()
local debug_elf = project_root .. "/build/PRTN.ino.elf"
local arduino_clangd = project_root .. "/tools/clangd-esp32"
local clangd_db_dir = project_root .. "/.clangd-db"
local clangd_db_path = clangd_db_dir .. "/compile_commands.json"

local function newest_glob(pattern)
	local matches = vim.fn.glob(vim.fn.expand(pattern), true, true)
	table.sort(matches)
	return matches[#matches] or ""
end

local codelldb = vim.fn.exepath("codelldb")

if clangd == "" then
	clangd = "clangd"
end
if arduino_cli == "" then
	arduino_cli = "arduino-cli"
end
if arduino_ls == "" then
	arduino_ls = "arduino-language-server"
end
if codelldb == "" then
	codelldb = newest_glob("~/.local/share/nvim/mason/packages/codelldb/extension/adapter/codelldb")
end
if vim.fn.executable(arduino_clangd) == 0 then
	arduino_clangd = clangd
end

local function file_exists(path)
	return path and path ~= "" and vim.fn.filereadable(path) == 1
end

local function is_source_arg(arg)
	return type(arg) == "string" and arg:match("%.%a+$") and not arg:match("^%-")
end

local function clone_command(entry)
	local copy = {
		directory = entry.directory,
		file = entry.file,
	}

	if entry.arguments then
		copy.arguments = vim.deepcopy(entry.arguments)
	else
		copy.command = entry.command
	end

	return copy
end

local function command_args(entry)
	if entry.arguments then
		return entry.arguments
	end

	if entry.command then
		return vim.split(entry.command, "%s+")
	end

	return nil
end

local function replace_source_arg(args, old_file, new_file)
	for i, arg in ipairs(args) do
		if arg == old_file then
			args[i] = new_file
			return
		end
	end

	for i, arg in ipairs(args) do
		if is_source_arg(arg) then
			args[i] = new_file
		end
	end
end

local function remove_unsupported_flags(args)
	local unsupported = {
		["-fstrict-volatile-bitfields"] = true,
		["-fno-tree-switch-conversion"] = true,
	}

	for i = #args, 1, -1 do
		if unsupported[args[i]] then
			table.remove(args, i)
		end
	end
end

local esp_cxx_system_includes = nil

local function get_esp_cxx_system_includes()
	if esp_cxx_system_includes then
		return esp_cxx_system_includes
	end

	esp_cxx_system_includes = {}
	local seen = {}
	local roots = vim.fn.glob(vim.fn.expand("~/.arduino15/packages/esp32/tools/esp-rv32/*/riscv32-esp-elf/include/c++/*"), true, true)

	local function add_dir(path)
		if path and path ~= "" and vim.fn.isdirectory(path) == 1 and not seen[path] then
			seen[path] = true
			table.insert(esp_cxx_system_includes, path)
		end
	end

	for _, root in ipairs(roots) do
		add_dir(root)
		add_dir(root .. "/riscv32-esp-elf")
		add_dir(root .. "/riscv32-esp-elf/.")
		add_dir(root .. "/backward")
	end

	for _, sysroot in ipairs(vim.fn.glob(vim.fn.expand("~/.arduino15/packages/esp32/tools/esp-rv32/*/riscv32-esp-elf/include"), true, true)) do
		add_dir(sysroot)
	end

	return esp_cxx_system_includes
end

local function add_esp_cxx_system_includes(args)
	local insert_at = #args + 1

	for i = 2, #args do
		if is_source_arg(args[i]) then
			insert_at = i
			break
		end
	end

	for i = #get_esp_cxx_system_includes(), 1, -1 do
		local path = get_esp_cxx_system_includes()[i]
		local exists = false
		for _, arg in ipairs(args) do
			if arg == path then
				exists = true
				break
			end
		end

		if not exists then
			table.insert(args, insert_at, path)
			table.insert(args, insert_at, "-isystem")
		end
	end
end

local function expand_response_files(args)
	for i = #args, 1, -1 do
		local path = type(args[i]) == "string" and args[i]:match("^@(.+)$")
		if path and file_exists(path) then
			local content = table.concat(vim.fn.readfile(path), " ")
			local expanded = vim.split(content, "%s+", { trimempty = true })
			table.remove(args, i)
			for j = #expanded, 1, -1 do
				table.insert(args, i, expanded[j])
			end
		end
	end
end

local function ensure_cxx_header_flags(args, header)
	local file_index = nil

	for i, arg in ipairs(args) do
		if arg == header then
			file_index = i
			break
		end
	end

	if not file_index then
		return
	end

	for _, arg in ipairs(args) do
		if arg == "-x" then
			return
		end
	end

	table.insert(args, file_index, "c++-header")
	table.insert(args, file_index, "-x")
end

local function write_clangd_db()
	local source_db = project_root .. "/compile_commands.json"
	if not file_exists(source_db) then
		return
	end

	local ok, decoded = pcall(vim.json.decode, table.concat(vim.fn.readfile(source_db), "\n"))
	if not ok or type(decoded) ~= "table" then
		return
	end

	local output = {}
	local seen = {}
	local cxx_core_entry = nil

	local function add(entry)
		local args = command_args(entry)
		if args then
			expand_response_files(args)
			remove_unsupported_flags(args)
			add_esp_cxx_system_includes(args)
		end

		if entry.file and not seen[entry.file] then
			seen[entry.file] = true
			table.insert(output, entry)
		end
	end

	for _, entry in ipairs(decoded) do
		if type(entry) == "table" and entry.file then
			local normalized = clone_command(entry)
			add(normalized)

			if entry.file:match("/build/sketch/src/.+%.c[cp]p$") then
				local source_file = entry.file:gsub("/build/sketch/src/", "/src/")
				local mapped = clone_command(entry)
				mapped.file = source_file
				local args = command_args(mapped)
				if args then
					replace_source_arg(args, entry.file, source_file)
				end
				add(mapped)
			end

			if entry.file:match("/hardware/esp32/[^/]+/cores/esp32/.+%.cpp$") and not cxx_core_entry then
				cxx_core_entry = entry
			end
		end
	end

	if cxx_core_entry then
		local core_dir = cxx_core_entry.file:match("(.+)/[^/]+%.cpp$")
		local headers = vim.fn.glob(core_dir .. "/*.h", true, true)
		for _, header in ipairs(headers) do
			local header_entry = clone_command(cxx_core_entry)
			header_entry.file = header
			local args = command_args(header_entry)
			if args then
				replace_source_arg(args, cxx_core_entry.file, header)
				ensure_cxx_header_flags(args, header)
			end
			add(header_entry)
		end
	end

	vim.fn.mkdir(clangd_db_dir, "p")
	vim.fn.writefile(vim.split(vim.json.encode(output), "\n"), clangd_db_path)
end

write_clangd_db()

vim.filetype.add({
	extension = {
		ino = "arduino",
		pde = "arduino",
		h = "cpp",
		hpp = "cpp",
		cpp = "cpp",
		c = "c",
	},
})

local function source_path(bufnr_or_fname)
	if type(bufnr_or_fname) == "number" then
		return vim.api.nvim_buf_get_name(bufnr_or_fname)
	end
	return bufnr_or_fname
end

local function prtn_root_path(bufnr_or_fname)
	local fname = source_path(bufnr_or_fname)
	local root = vim.fs.root(fname, {
		".clangd",
		"compile_commands.json",
		"PRTN.ino",
		"Makefile",
		".git",
	})

	if root then
		return root
	end

	local sketch = vim.fs.find(function(name)
		return name:match("%.ino$")
	end, {
		path = fname ~= "" and (vim.fn.filereadable(fname) == 1 and vim.fs.dirname(fname) or fname) or vim.fn.getcwd(),
		upward = true,
		limit = 1,
	})[1]

	return sketch and vim.fs.dirname(sketch) or vim.fn.getcwd()
end

local function prtn_root(bufnr_or_fname, on_dir)
	local root = prtn_root_path(bufnr_or_fname)
	if on_dir then
		on_dir(root)
	else
		return root
	end
end

return {
	{
		"mfussenegger/nvim-dap",
		keys = {
			{
				"<leader>db",
				function()
					require("dap").toggle_breakpoint()
				end,
				desc = "Toggle breakpoint",
			},
			{
				"<leader>dc",
				function()
					require("dap").continue()
				end,
				desc = "Debug continue",
			},
			{
				"<leader>di",
				function()
					require("dap").step_into()
				end,
				desc = "Step into",
			},
			{
				"<leader>do",
				function()
					require("dap").step_over()
				end,
				desc = "Step over",
			},
			{
				"<leader>dO",
				function()
					require("dap").step_out()
				end,
				desc = "Step out",
			},
			{
				"<leader>dr",
				function()
					require("dap").repl.open()
				end,
				desc = "Debug REPL",
			},
		},
		config = function()
			local dap = require("dap")

			if vim.fn.executable(codelldb) == 0 then
				vim.notify("codelldb not found. Run :MasonInstall codelldb for ESP32-C3 nvim-dap debugging.", vim.log.levels.WARN)
				return
			end

			dap.adapters.codelldb = {
				type = "server",
				port = "${port}",
				executable = {
					command = codelldb,
					args = { "--port", "${port}" },
				},
			}

			local esp32c3_attach = {
				name = "ESP32-C3: Attach OpenOCD",
				type = "codelldb",
				request = "launch",
				cwd = project_root,
				targetCreateCommands = {
					"target create " .. debug_elf,
				},
				processCreateCommands = {
					"gdb-remote localhost:3333",
				},
			}

			dap.configurations.c = { esp32c3_attach }
			dap.configurations.cpp = { esp32c3_attach }
			dap.configurations.arduino = { esp32c3_attach }
		end,
	},
	{
		"neovim/nvim-lspconfig",
		opts = {
			servers = {
				arduino_language_server = {
					cmd = {
						arduino_ls,
						"-clangd",
						arduino_clangd,
						"-cli",
						arduino_cli,
						"-cli-config",
						vim.fn.expand("~/.arduino15/arduino-cli.yaml"),
						"-fqbn",
						fqbn,
					},
					filetypes = { "arduino" },
					root_dir = prtn_root,
					single_file_support = true,
					get_language_id = function(_, filetype)
						return filetype == "arduino" and "cpp" or filetype
					end,
				},

				clangd = {
					cmd = {
						clangd,
						"--background-index",
						"--clang-tidy",
						"--header-insertion=iwyu",
						"--completion-style=detailed",
						"--function-arg-placeholders",
						"--fallback-style=llvm",
						"--compile-commands-dir=" .. clangd_db_dir,
						"--query-driver=" .. esp_rv32_query_driver,
					},
					filetypes = {
						"c",
						"cpp",
						"arduino",
						"objc",
						"objcpp",
						"cuda",
					},
					root_dir = prtn_root,
					single_file_support = true,
				},
			},
		},
	},
}
