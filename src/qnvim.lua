-- qnvim's Lua API.
--
-- qnvim gives this module to Neovim before Neovim loads the user's
-- configuration (as a --cmd, through package.preload), so it can be required
-- from init.lua, qnvim.vim and plugins:
--
--     local qnvim = require('qnvim')
--     vim.keymap.set('n', '<leader>b', qnvim.build)
--
-- The functions notify Qt Creator over qnvim's RPC channel, which qnvim sets
-- when it attaches to Neovim; calling them earlier raises an error.
-- See docs/configuration.md.
local M = {}

local function notify(...)
  local channel = vim.g.neovim_channel
  if not channel then
    error('qnvim has not attached to Neovim yet', 3)
  end
  vim.rpcnotify(channel, 'Gui', ...)
end

--- Trigger one or more Qt Creator commands by id, in order. An id that does
--- not match a registered command is ignored and logged.
--- @param ... string Qt Creator command ids, for example 'ProjectExplorer.Build'.
function M.trigger_command(...)
  notify('triggerCommand', ...)
end

-- The functions below mirror qnvim's Ex commands.

--- Build the current project (:Build).
function M.build() notify('triggerCommand', 'ProjectExplorer.Build') end

--- Build the session (:BuildAll).
function M.build_all() notify('triggerCommand', 'ProjectExplorer.BuildSession') end

--- Rebuild the current project (:Rebuild).
function M.rebuild() notify('triggerCommand', 'ProjectExplorer.Rebuild') end

--- Rebuild the session (:RebuildAll).
function M.rebuild_all() notify('triggerCommand', 'ProjectExplorer.RebuildSession') end

--- Clean the current project (:Clean).
function M.clean() notify('triggerCommand', 'ProjectExplorer.Clean') end

--- Clean the session (:CleanAll).
function M.clean_all() notify('triggerCommand', 'ProjectExplorer.CleanSession') end

--- Deploy the current project (:Deploy).
function M.deploy() notify('triggerCommand', 'ProjectExplorer.Deploy') end

--- Deploy the session (:DeployAll).
function M.deploy_all() notify('triggerCommand', 'ProjectExplorer.DeploySession') end

--- Run the current project (:Run).
function M.run() notify('triggerCommand', 'ProjectExplorer.Run') end

--- Start debugging (:Debug).
function M.debug() notify('triggerCommand', 'ProjectExplorer.Debug') end

--- Continue debugging (:DebugContinue).
function M.debug_continue() notify('triggerCommand', 'Debugger.Continue') end

--- Run qmake (:QMake).
function M.qmake() notify('triggerCommand', 'Qt4Builder.RunQMake') end

--- Select the build target (:Target).
function M.target() notify('triggerCommand', 'ProjectExplorer.SelectTargetQuick') end

return M
