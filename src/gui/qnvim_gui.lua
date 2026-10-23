-- qnvim GUI mode: glue between the embedded Neovim and Qt Creator.
--
-- Loaded with `--cmd luafile <this file>` before the user's init file, so it
-- does not depend on 'runtimepath' (plugin managers such as lazy.nvim reset
-- it). Qt Creator sets `vim.g.qnvim_gui_channel` before attaching the UI; until
-- then notifications are dropped.
--
-- Protocol (Neovim -> Qt Creator), all through
--   rpcnotify(g:qnvim_gui_channel, 'QNVimGui', <event>, ...):
--   'bufenter',    path                      a file buffer became current
--   'bufdelete',   path                      a file buffer was deleted/wiped
--   'modified',    path, is_modified         'modified' changed
--   'written',     path                      the buffer was written
--   'cursor',      path, line, utf16_col     the cursor moved (debounced)
--   'action',      action_id                 trigger a Qt Creator action
--   'actions'                                ask for the action list again
--
-- Functions Qt Creator calls (luaeval):
--   QNVimGui.open(path, line, col, center)   show path in a normal window
--   QNVimGui.add(path)                       make sure a buffer exists
--   QNVimGui.close(path)                     delete the buffer (discarding)
--   QNVimGui.write(path)                     write the buffer, returns '' or error
--   QNVimGui.set_actions(list)               [[id, description], ...] for :Qtc

local M = {}
_G.QNVimGui = M

vim.g.QNVIM = 1
vim.g.QNVIM_GUI = 1

local function notify(...)
  local chan = vim.g.qnvim_gui_channel
  if not chan or chan == 0 then
    return
  end
  pcall(vim.rpcnotify, chan, 'QNVimGui', ...)
end

-- A buffer Qt Creator should mirror: a listed, normal file buffer with a name.
local function file_path(buf)
  if not vim.api.nvim_buf_is_valid(buf) then
    return nil
  end
  if vim.bo[buf].buftype ~= '' or not vim.bo[buf].buflisted then
    return nil
  end
  local name = vim.api.nvim_buf_get_name(buf)
  if name == '' or name:match('^%a[%w+.-]*://') then
    return nil -- unnamed or a URI buffer (fugitive://, oil://, ...)
  end
  return vim.fn.fnamemodify(name, ':p')
end

local function normalize(path)
  return vim.fn.fnamemodify(path, ':p')
end

local function find_buf(path)
  local b = vim.fn.bufnr('^' .. vim.fn.escape(normalize(path), '\\/.*$^~[]') .. '$')
  if b > 0 then
    return b
  end
  b = vim.fn.bufnr(normalize(path))
  return b > 0 and b or nil
end

-- utf-16 code unit column (Qt) <-> byte column (Neovim)
local function to_byte_col(line_text, utf16_col)
  if utf16_col <= 0 then
    return 0
  end
  local ok, res = pcall(vim.str_byteindex, line_text, 'utf-16', utf16_col, false)
  if ok and type(res) == 'number' then
    return res
  end
  ok, res = pcall(vim.str_byteindex, line_text, utf16_col, true) -- Neovim < 0.11
  if ok and type(res) == 'number' then
    return res
  end
  return math.min(utf16_col, #line_text)
end

local function to_utf16_col(line_text, byte_col)
  local ok, _, utf16 = pcall(vim.str_utfindex, line_text, math.min(byte_col, #line_text))
  if ok and type(utf16) == 'number' then
    return utf16
  end
  return byte_col
end

-- The window to show a file in: the current one unless it is special
-- (floating, a file tree, quickfix, terminal, 'winfixbuf').
local function normal_window()
  local function usable(win)
    local cfg = vim.api.nvim_win_get_config(win)
    if cfg.relative and cfg.relative ~= '' then
      return false
    end
    local buf = vim.api.nvim_win_get_buf(win)
    if vim.bo[buf].buftype ~= '' then
      return false
    end
    local ok, fix = pcall(function() return vim.wo[win].winfixbuf end)
    return not (ok and fix)
  end
  local cur = vim.api.nvim_get_current_win()
  if usable(cur) then
    return cur
  end
  local alt = vim.fn.win_getid(vim.fn.winnr('#'))
  if alt ~= 0 and usable(alt) then
    return alt
  end
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if usable(win) then
      return win
    end
  end
  return nil
end

function M.open(path, line, col, center)
  path = normalize(path)
  local buf = find_buf(path)
  -- Already shown in a window of this tab: jump there (like :drop).
  if buf then
    for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
      if vim.api.nvim_win_get_buf(win) == buf then
        vim.api.nvim_set_current_win(win)
        buf = nil
        break
      end
    end
  end
  if vim.api.nvim_buf_get_name(0) ~= path then
    local win = normal_window()
    if win then
      vim.api.nvim_set_current_win(win)
    else
      vim.cmd('botright vsplit')
    end
    local ok, err = pcall(vim.cmd, 'edit ' .. vim.fn.fnameescape(path))
    if not ok then
      vim.notify('qnvim: ' .. tostring(err), vim.log.levels.ERROR)
      return
    end
  end
  if line and line > 0 then
    local count = vim.api.nvim_buf_line_count(0)
    local l = math.min(line, count)
    local text = vim.api.nvim_buf_get_lines(0, l - 1, l, false)[1] or ''
    pcall(vim.api.nvim_win_set_cursor, 0, { l, to_byte_col(text, col or 0) })
    if center then
      vim.cmd('normal! zz')
    end
  end
end

function M.add(path)
  path = normalize(path)
  if not find_buf(path) then
    vim.fn.bufadd(path)
    local b = find_buf(path)
    if b then
      vim.bo[b].buflisted = true
    end
  end
end

function M.close(path)
  local buf = find_buf(path)
  if buf and vim.api.nvim_buf_is_valid(buf) then
    M._closing = buf
    pcall(vim.api.nvim_buf_delete, buf, { force = true })
    M._closing = nil
  end
end

function M.write(path)
  local buf = find_buf(path)
  if not buf or not vim.api.nvim_buf_is_loaded(buf) then
    return ''
  end
  if not vim.bo[buf].modified then
    return ''
  end
  local ok, err = pcall(vim.api.nvim_buf_call, buf, function()
    vim.cmd('silent write')
  end)
  return ok and '' or tostring(err)
end

-- :Qtc {action} -----------------------------------------------------------

M.actions = {}

function M.set_actions(list)
  M.actions = list or {}
end

local function action_ids()
  local ids = {}
  for _, a in ipairs(M.actions) do
    ids[#ids + 1] = a[1]
  end
  return ids
end

vim.api.nvim_create_user_command('Qtc', function(opts)
  if opts.args == '' then
    -- No id: pick one. Uses vim.ui.select, i.e. telescope/fzf-lua/snacks if
    -- the user configured them as ui-select provider.
    vim.ui.select(M.actions, {
      prompt = 'Qt Creator action',
      format_item = function(a)
        return (a[2] ~= '' and (a[2] .. '  ') or '') .. '[' .. a[1] .. ']'
      end,
    }, function(choice)
      if choice then
        notify('action', choice[1])
      end
    end)
    return
  end
  notify('action', opts.args)
end, {
  nargs = '?',
  desc = 'Trigger a Qt Creator action (no argument: pick one)',
  complete = function(lead)
    return vim.tbl_filter(function(id)
      return vim.startswith(id:lower(), lead:lower())
    end, action_ids())
  end,
})

-- Short commands, same names as in qnvim's classic mode.
local shortcuts = {
  Build = 'ProjectExplorer.Build',
  BuildProject = 'ProjectExplorer.Build',
  BuildAll = 'ProjectExplorer.BuildSession',
  Rebuild = 'ProjectExplorer.Rebuild',
  RebuildAll = 'ProjectExplorer.RebuildSession',
  Clean = 'ProjectExplorer.Clean',
  CleanAll = 'ProjectExplorer.CleanSession',
  Deploy = 'ProjectExplorer.Deploy',
  Run = 'ProjectExplorer.Run',
  Debug = 'ProjectExplorer.Debug',
  DebugContinue = 'Debugger.Continue',
  DebugStop = 'Debugger.Stop',
  DebugStepOver = 'Debugger.NextLine',
  DebugStepInto = 'Debugger.StepLine',
  DebugStepOut = 'Debugger.StepOut',
  Target = 'ProjectExplorer.SelectTargetQuick',
  Locator = 'QtCreator.Locate',
  QtcFind = 'Find.Dialog',
}
for name, id in pairs(shortcuts) do
  if vim.fn.exists(':' .. name) ~= 2 then
    vim.api.nvim_create_user_command(name, function()
      notify('action', id)
    end, { bar = true, desc = 'Qt Creator: ' .. id })
  end
end

-- Autocommands --------------------------------------------------------------

local group = vim.api.nvim_create_augroup('qnvim_gui', { clear = true })

vim.api.nvim_create_autocmd('BufEnter', {
  group = group,
  callback = function(ev)
    if vim.api.nvim_win_get_config(0).relative ~= '' then
      return -- floats (pickers, hovers) do not change Qt Creator's editor
    end
    local p = file_path(ev.buf)
    if p then
      notify('bufenter', p)
    end
  end,
})

vim.api.nvim_create_autocmd({ 'BufDelete', 'BufWipeout' }, {
  group = group,
  callback = function(ev)
    if M._closing == ev.buf then
      return -- Qt Creator asked for this
    end
    local name = vim.api.nvim_buf_get_name(ev.buf)
    if name ~= '' and vim.bo[ev.buf].buftype == '' then
      notify('bufdelete', normalize(name))
    end
  end,
})

vim.api.nvim_create_autocmd('BufModifiedSet', {
  group = group,
  callback = function(ev)
    local p = file_path(ev.buf)
    if p then
      notify('modified', p, vim.bo[ev.buf].modified)
    end
  end,
})

vim.api.nvim_create_autocmd('BufWritePost', {
  group = group,
  callback = function(ev)
    local p = file_path(ev.buf)
    if p then
      notify('written', p)
      notify('modified', p, vim.bo[ev.buf].modified)
    end
  end,
})

local cursor_timer = vim.uv and vim.uv.new_timer() or vim.loop.new_timer()
vim.api.nvim_create_autocmd({ 'CursorMoved', 'CursorMovedI' }, {
  group = group,
  callback = function(ev)
    local p = file_path(ev.buf)
    if not p then
      return
    end
    cursor_timer:stop()
    cursor_timer:start(150, 0, vim.schedule_wrap(function()
      if not vim.api.nvim_buf_is_valid(ev.buf) or vim.api.nvim_get_current_buf() ~= ev.buf then
        return
      end
      local pos = vim.api.nvim_win_get_cursor(0)
      local text = vim.api.nvim_get_current_line()
      notify('cursor', p, pos[1], to_utf16_col(text, pos[2]))
    end))
  end,
})

-- Optional per-host config, like classic qnvim: <config>/qnvim.vim or
-- <config>/qnvim.lua is sourced after the user's init file.
vim.api.nvim_create_autocmd('VimEnter', {
  group = group,
  once = true,
  callback = function()
    local cfg = vim.fn.stdpath('config')
    for _, f in ipairs({ cfg .. '/qnvim.lua', cfg .. '/qnvim.vim' }) do
      if vim.fn.filereadable(f) == 1 then
        pcall(vim.cmd, 'source ' .. vim.fn.fnameescape(f))
      end
    end
    -- Report the buffer Neovim starts with (e.g. a session restore).
    local p = file_path(0)
    if p then
      notify('bufenter', p)
    end
  end,
})

return M
