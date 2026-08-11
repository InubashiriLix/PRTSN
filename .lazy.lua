-- Project-local LazyVim configuration for PRTN.
-- PRTN = PRTS Node

if vim.lsp.log and vim.lsp.log.set_level then
	vim.lsp.log.set_level(vim.log.levels.ERROR)
else
	vim.lsp.set_log_level("ERROR")
end

local clangd = vim.fn.exepath("clangd")

local project_root = vim.fs.root(vim.fn.getcwd(), { ".lazy.lua", "PRTN.ino", "compile_commands.json" })
	or vim.fn.getcwd()
local debug_elf = project_root .. "/build/PRTN.ino.elf"
local clangd_state_dir = project_root .. "/.clangd-db"
local include_signature_path = clangd_state_dir .. "/include-signature"

local function newest_glob(pattern)
	local matches = vim.fn.glob(vim.fn.expand(pattern), true, true)
	table.sort(matches)
	return matches[#matches] or ""
end

local codelldb = vim.fn.exepath("codelldb")

if clangd == "" then
	clangd = "clangd"
end
if codelldb == "" then
	codelldb = newest_glob("~/.local/share/nvim/mason/packages/codelldb/extension/adapter/codelldb")
end

local esp32_tools = vim.fn.expand("~/.arduino15/packages/esp32/tools")
local query_drivers = table.concat({
	esp32_tools .. "/esp-x32/*/bin/*",
	esp32_tools .. "/esp-rv32/*/bin/*",
}, ",")

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

local source_extensions = {
	c = true,
	cc = true,
	cpp = true,
	cxx = true,
	h = true,
	hh = true,
	hpp = true,
	hxx = true,
	ino = true,
}

local ignored_directories = {
	[".clangd-db"] = true,
	[".git"] = true,
	build = true,
}

local function include_signature()
	local includes = {}

	local function walk(directory, relative_directory)
		for name, kind in vim.fs.dir(directory) do
			local relative = relative_directory == "" and name or (relative_directory .. "/" .. name)
			local path = directory .. "/" .. name

			if kind == "directory" and not ignored_directories[name] then
				walk(path, relative)
			elseif kind == "file" then
				local extension = name:match("%.([^.]+)$")
				if extension and source_extensions[extension:lower()] then
					local ok, lines = pcall(vim.fn.readfile, path)
					if ok then
						for _, line in ipairs(lines) do
							local include = line:match('^%s*#%s*include%s*[<"].+')
							if include then
								table.insert(includes, relative .. "\0" .. vim.trim(include))
							end
						end
					end
				end
			end
		end
	end

	walk(project_root, "")
	table.sort(includes)
	return vim.fn.sha256(table.concat(includes, "\n"))
end

local function read_saved_signature()
	if vim.fn.filereadable(include_signature_path) ~= 1 then
		return nil
	end

	return vim.trim(table.concat(vim.fn.readfile(include_signature_path), "\n"))
end

local function write_saved_signature(signature)
	vim.fn.mkdir(clangd_state_dir, "p")
	vim.fn.writefile({ signature }, include_signature_path)
end

local function restart_project_clangd()
	local restart_config = nil
	local attached_buffers = {}
	for _, client in ipairs(vim.lsp.get_clients({ name = "clangd" })) do
		if vim.fs.normalize(client.config.root_dir or "") == vim.fs.normalize(project_root) then
			restart_config = restart_config or client.config
			for _, bufnr in ipairs(vim.api.nvim_list_bufs()) do
				if vim.lsp.buf_is_attached(bufnr, client.id) then
					table.insert(attached_buffers, bufnr)
				end
			end
			client:stop()
		end
	end

	if not restart_config or not attached_buffers[1] then
		return
	end

	vim.defer_fn(function()
		local client_id = vim.lsp.start(restart_config, { bufnr = attached_buffers[1] })
		if not client_id then
			vim.notify("PRTN: failed to restart clangd", vim.log.levels.ERROR)
			return
		end

		for index = 2, #attached_buffers do
			local bufnr = attached_buffers[index]
			if vim.api.nvim_buf_is_valid(bufnr) and vim.api.nvim_buf_is_loaded(bufnr) then
				vim.lsp.buf_attach_client(bufnr, client_id)
			end
		end
	end, 200)
end

local refresh_running = false
local refresh_pending = false
local refresh_timer = nil
local last_signature = read_saved_signature()

local function refresh_compdb(force)
	local requested_signature = include_signature()
	if not force and requested_signature == last_signature then
		return
	end

	if refresh_running then
		refresh_pending = true
		return
	end

	refresh_running = true
	vim.notify("PRTN: refreshing clangd compile database...", vim.log.levels.INFO)

	vim.system({ "make", "--no-print-directory", "compdb" }, {
		cwd = project_root,
		text = true,
	}, function(result)
		vim.schedule(function()
			refresh_running = false
			local current_signature = include_signature()

			if result.code ~= 0 then
				local output = vim.trim((result.stdout or "") .. "\n" .. (result.stderr or ""))
				vim.notify("PRTN: compile database refresh failed\n" .. output, vim.log.levels.ERROR)
				return
			end

			if current_signature ~= requested_signature or refresh_pending then
				refresh_pending = false
				refresh_compdb(true)
				return
			end

			last_signature = current_signature
			write_saved_signature(current_signature)
			restart_project_clangd()
			vim.notify("PRTN: clangd compile database refreshed", vim.log.levels.INFO)
		end)
	end)
end

local function schedule_compdb_refresh()
	if refresh_timer then
		refresh_timer:stop()
		refresh_timer:close()
	end

	refresh_timer = vim.uv.new_timer()
	refresh_timer:start(
		350,
		0,
		vim.schedule_wrap(function()
			refresh_timer:stop()
			refresh_timer:close()
			refresh_timer = nil
			refresh_compdb(false)
		end)
	)
end

local refresh_group = vim.api.nvim_create_augroup("PrtnClangdCompdb", { clear = true })
vim.api.nvim_create_autocmd("BufWritePost", {
	group = refresh_group,
	pattern = { "*.c", "*.cc", "*.cpp", "*.cxx", "*.h", "*.hh", "*.hpp", "*.hxx", "*.ino" },
	callback = schedule_compdb_refresh,
})
vim.api.nvim_create_autocmd("VimEnter", {
	group = refresh_group,
	once = true,
	callback = function()
		if
			vim.fn.filereadable(project_root .. "/compile_commands.json") ~= 1
			or include_signature() ~= last_signature
		then
			refresh_compdb(true)
		end
	end,
})

vim.api.nvim_create_user_command("PrtnCompdb", function()
	refresh_compdb(true)
end, { desc = "Regenerate the PRTN clangd compile database" })

vim.api.nvim_create_user_command("LspRestart", function(options)
	if options.args ~= "" and options.args ~= "clangd" then
		vim.notify("PRTN: only the clangd client is configured for this project", vim.log.levels.ERROR)
		return
	end

	restart_project_clangd()
end, {
	desc = "Restart the PRTN clangd client",
	nargs = "?",
	complete = function()
		return { "clangd" }
	end,
	force = true,
})

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
				vim.notify(
					"codelldb not found. Run :MasonInstall codelldb for ESP32-C3 nvim-dap debugging.",
					vim.log.levels.WARN
				)
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
				arduino_language_server = { enabled = false },
				clangd = {
					cmd = {
						clangd,
						"--background-index",
						"--clang-tidy",
						"--header-insertion=iwyu",
						"--completion-style=detailed",
						"--function-arg-placeholders",
						"--fallback-style=llvm",
						"--compile-commands-dir=" .. project_root,
						"--query-driver=" .. query_drivers,
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
