-- qnvim's example configuration in Lua.
--
-- Put this file next to your init.lua as qnvim.lua; qnvim sets $MYQVIMRC to it
-- and sources it at startup. It uses qnvim's Lua API:
--
--     local qnvim = require('qnvim')
--     qnvim.build()
--
-- See docs/configuration.md.
local qnvim = require('qnvim')

local map = vim.keymap.set
local unpack = table.unpack or unpack

-- A mapping or command callback that triggers one or more Qt Creator commands.
local function trigger(...)
  local ids = { ... }
  return function()
    qnvim.trigger_command(unpack(ids))
  end
end

-- Build, deploy, run and select the build target.
map('n', '<D-b>', qnvim.build)
map('n', '<D-d>', qnvim.deploy)
map('n', '<D-r>', qnvim.run)
map('n', '<D-t>', qnvim.target)

-- Combined actions, like :Build|Deploy.
local function command(name, action)
  vim.api.nvim_create_user_command(name, action, { force = true })
end

command('B', qnvim.build)
command('BD', trigger('ProjectExplorer.Build', 'ProjectExplorer.Deploy'))
command('BR', trigger('ProjectExplorer.Build', 'ProjectExplorer.Run'))
command('BDR', trigger('ProjectExplorer.Build', 'ProjectExplorer.Deploy', 'ProjectExplorer.Run'))
command('D', qnvim.deploy)
command('DR', trigger('ProjectExplorer.Deploy', 'ProjectExplorer.Run'))
command('R', qnvim.run)
command('Q', qnvim.qmake)

map('n', 'qq', qnvim.qmake, { silent = true })
map('n', '1q', qnvim.build, { silent = true })
map('n', '2q', qnvim.deploy, { silent = true })
map('n', '3q', trigger('ProjectExplorer.Build', 'ProjectExplorer.Deploy'), { silent = true })
map('n', '4q', qnvim.run, { silent = true })
map('n', '5q', trigger('ProjectExplorer.Build', 'ProjectExplorer.Run'), { silent = true })
map('n', '6q', trigger('ProjectExplorer.Deploy', 'ProjectExplorer.Run'), { silent = true })
map('n', '7q', trigger('ProjectExplorer.Build', 'ProjectExplorer.Deploy', 'ProjectExplorer.Run'), { silent = true })

map('n', '==', trigger('TextEditor.AutoIndentSelection'), { silent = true })
map('v', '=', trigger('TextEditor.AutoIndentSelection'), { silent = true })
map('n', '<C-]>', trigger('TextEditor.FollowSymbolUnderCursor', 'TextEditor.JumpToFileUnderCursor'), { silent = true })
map('n', '<F4>', trigger('CppTools.SwitchHeaderSource'), { silent = true })
map('n', '<D-w>', trigger('QtCreator.Close', 'TextEditor.JumpToFileUnderCursor'), { silent = true })
map('n', '<C-Tab>', trigger('QtCreator.GotoPreviousInHistory'), { silent = true })
map('n', '<C-S-Tab>', trigger('QtCreator.GotoPreviousInHistory'), { silent = true })
map('i', '<C-Space>', function()
  qnvim.trigger_command('TextEditor.CompleteThis')
  return '<C-o><Esc>'
end, { silent = true, expr = true })

map('n', '<D-1>', trigger('QtCreator.Pane.Issues'), { silent = true })
map('n', '<D-2>', trigger('QtCreator.Pane.SearchResults'), { silent = true })
map('n', '<D-3>', trigger('QtCreator.Pane.ApplicationOutput'), { silent = true })
map('n', '<D-4>', trigger('QtCreator.Pane.CompileOutput'), { silent = true })
map('n', '<D-5>', trigger('QtCreator.Pane.DebuggerConsole'), { silent = true })
map('n', '<D-6>', trigger('QtCreator.Pane.To-DoEntries'), { silent = true })
map('n', '<D-7>', trigger('QtCreator.Pane.GeneralMessages'), { silent = true })
map('n', '<D-8>', trigger('QtCreator.Pane.VersionControl'), { silent = true })

map('n', '<C-1>', trigger('QtCreator.Mode.Welcome'), { silent = true })
map('n', '<C-2>', trigger('QtCreator.Mode.Edit'), { silent = true })
map('n', '<C-3>', trigger('QtCreator.Mode.Design'), { silent = true })
map('n', '<C-4>', trigger('QtCreator.Mode.Debug'), { silent = true })
map('n', '<C-5>', trigger('QtCreator.Mode.Project'), { silent = true })
map('n', '<C-6>', trigger('QtCreator.Mode.Help'), { silent = true })
map('n', '<C-7>', trigger('QtCreator.Mode.Welcome'), { silent = true })

map('n', '<F1>', trigger('Help.Context'), { silent = true })

map('n', '<D-0>', trigger('QtCreator.ToggleLeftSidebar'), { silent = true })
map('n', '<D-A-0>', trigger('QtCreator.ToggleModeSelector'), { silent = true })
map('n', '<D-S-0>', trigger('QtCreator.ToggleRightSidebar'), { silent = true })
map('n', '<D-S-F>', trigger('QtCreator.ToggleFullScreen'), { silent = true })

map('n', '<A-CR>', trigger('TextEditor.QuickFix'), { silent = true })
map('n', '<D-,>', trigger('QtCreator.Options'), { silent = true })

map('n', '<A-V><A-V>', trigger('QNVim.Toggle'), { silent = true })

pcall(vim.keymap.del, { 'n', 'v', 'o' }, '<Tab>')

vim.opt.relativenumber = false
