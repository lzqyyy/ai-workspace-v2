'use strict';

const $ = (id) => document.getElementById(id);
const els = {
  auth: $('auth-view'), workspace: $('workspace-view'), loginForm: $('login-form'), loginButton: $('login-button'), loginError: $('login-error'),
  logout: $('logout-button'), userName: $('user-name'), userAvatar: $('user-avatar'), userRole: $('user-role'),
  projectSelect: $('project-select'), newProject: $('new-project'), projectSettings: $('project-settings'), projectName: $('project-name'),
  projectDescription: $('project-description'), roleBadge: $('role-badge'), projectCreateDialog: $('project-create-dialog'),
  projectForm: $('project-form'), projectNameInput: $('project-name-input'), projectDescriptionInput: $('project-description-input'),
  projectError: $('project-error'), projectSubmit: $('project-submit'), projectSettingsDialog: $('project-settings-dialog'),
  settingsProjectName: $('settings-project-name'), memberList: $('member-list'), projectAgentList: $('project-agent-list'),
  showMemberForm: $('show-member-form'), memberForm: $('member-form'), memberUserId: $('member-user-id'), memberRole: $('member-role'),
  ownedAgentSelect: $('owned-agent-select'), addAgentControls: $('add-agent-controls'), addOwnedAgent: $('add-owned-agent'),
  settingsError: $('settings-error'), agentSelect: $('agent-select'), newAgent: $('new-agent'), newConversation: $('new-conversation'),
  conversationList: $('conversation-list'), conversationCount: $('conversation-count'), conversationTitle: $('conversation-title'),
  conversationSubtitle: $('conversation-subtitle'), runState: $('run-state'), runStateText: $('run-state-text'), messages: $('messages'),
  emptyState: $('empty-state'), emptyAction: $('empty-action'), messageForm: $('message-form'), messageInput: $('message-input'),
  sendButton: $('send-button'), cancelButton: $('cancel-button'), inlineError: $('inline-error'), inlineErrorText: $('inline-error-text'),
  retryButton: $('retry-button'), composerHint: $('composer-hint'), readonlyNote: $('readonly-note'), agentDialog: $('agent-dialog'),
  agentForm: $('agent-form'), agentName: $('agent-name'), agentPrompt: $('agent-prompt'), agentError: $('agent-error'),
  agentSubmit: $('agent-submit'), toast: $('toast')
};

const state = {
  user: null, projects: [], project: null, role: null, members: [], projectAgents: [], ownedAgents: [],
  conversations: [], activeConversation: null, messages: [], targetAgentId: null, run: null, eventSource: null,
  conversationEventSource: null, conversationEventCursor: 0,
  lastRunId: null, lastRunOwnerId: null, terminalRuns: new Set(), retrying: false, sending: false,
  cancelling: false, retryVisible: false, pollTimer: null, messageLoadSeq: 0, toastTimer: null
};

class ApiError extends Error {
  constructor(code, message, status, retryable = false) { super(message); this.code = code; this.status = status; this.retryable = retryable; }
}

function uuid() {
  if (crypto.randomUUID) return crypto.randomUUID();
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (character) => {
    const random = Math.random() * 16 | 0;
    return (character === 'x' ? random : (random & 3 | 8)).toString(16);
  });
}

async function api(path, options = {}) {
  const requestId = options.requestId || `web-${uuid()}`;
  const headers = new Headers(options.headers || {});
  headers.set('X-Request-ID', requestId);
  if (options.body && !headers.has('Content-Type')) headers.set('Content-Type', 'application/json');
  let response;
  try { response = await fetch(path, {...options, headers, credentials: 'same-origin'}); }
  catch { throw new ApiError('NETWORK_ERROR', '无法连接服务，请检查网络后重试', 0, true); }
  if (response.status === 204) return null;
  let data = null;
  try { data = await response.json(); } catch {}
  if (!response.ok) throw new ApiError(data?.code || 'INTERNAL', data?.message || '请求失败，请稍后重试', response.status, Boolean(data?.retryable));
  return data;
}

function canWrite() { return state.role === 'owner' || state.role === 'editor'; }
function canManage() { return state.role === 'owner'; }
function canCancelRun(run = state.run) { return canWrite() && Boolean(run) && (state.role === 'owner' || run.ownerId === state.user?.id); }
function canRetryLast() { return canWrite() && Boolean(state.lastRunId) && (state.role === 'owner' || state.lastRunOwnerId === state.user?.id); }
function activeRun() { return state.run && state.run.conversationId === state.activeConversation?.id ? state.run : null; }
function currentMember() { return state.members.find((member) => member.user_id === state.user?.id) || null; }

function showAuth() { stopPolling(); closeRunStream(); closeConversationStream(); els.workspace.classList.add('hidden'); els.auth.classList.remove('hidden'); els.loginButton.disabled = false; setTimeout(() => $('username').focus(), 20); }
function showWorkspace() { els.auth.classList.add('hidden'); els.workspace.classList.remove('hidden'); }
function toast(message) { clearTimeout(state.toastTimer); els.toast.textContent = message; els.toast.classList.remove('hidden'); state.toastTimer = setTimeout(() => els.toast.classList.add('hidden'), 2600); }
function formatTime(value) { if (!value) return ''; const date = new Date(value); if (Number.isNaN(date.getTime())) return ''; return new Intl.DateTimeFormat('zh-CN', {hour: '2-digit', minute: '2-digit'}).format(date); }
function setRunState(kind, text) { els.runState.className = `run-state ${kind || ''}`.trim(); els.runStateText.textContent = text; }
function setError(message, retryable = false) { els.inlineErrorText.textContent = message || ''; els.inlineError.classList.toggle('hidden', !message); state.retryVisible = Boolean(message && retryable && canRetryLast()); applyPermissions(); }
function clearError() { els.inlineErrorText.textContent = ''; els.inlineError.classList.add('hidden'); state.retryVisible = false; applyPermissions(); }

function applyPermissions() {
  const hasProject = Boolean(state.project);
  const hasConversation = Boolean(state.activeConversation);
  const hasAgents = state.projectAgents.length > 0;
  const viewer = hasProject && state.role === 'viewer';
  const locked = Boolean(state.run) || state.sending || state.cancelling;
  els.projectSelect.disabled = state.projects.length === 0;
  els.projectSettings.disabled = !hasProject;
  els.newConversation.disabled = !canWrite() || !hasAgents || locked;
  els.agentSelect.disabled = !hasProject || !hasAgents;
  els.messageInput.disabled = !canWrite() || !hasConversation || locked;
  els.sendButton.disabled = !canWrite() || !hasConversation || locked || !state.targetAgentId;
  els.cancelButton.classList.toggle('hidden', !canCancelRun());
  els.cancelButton.disabled = state.cancelling;
  els.retryButton.classList.toggle('hidden', !state.retryVisible);
  els.retryButton.disabled = state.retrying;
  els.readonlyNote.classList.toggle('hidden', !viewer);
  els.showMemberForm.classList.toggle('hidden', !canManage());
  els.addAgentControls.classList.toggle('hidden', !canWrite());
  els.messageInput.placeholder = viewer ? '当前为 viewer，只读查看共享会话' : state.run ? '当前已有运行中的任务' : '输入消息，Enter 发送，Shift + Enter 换行';
  if (viewer) els.composerHint.textContent = 'viewer 只读 · SSE 实时查看';
  else if (state.role) els.composerHint.textContent = `${state.role} · target_agent_id 由 Project Agent 选择器提交`;
}
function setSending(busy) { state.sending = Boolean(busy); applyPermissions(); }

async function login(event) {
  event.preventDefault(); els.loginError.textContent = ''; els.loginButton.disabled = true;
  try {
    await api('/api/v1/auth/login', {method: 'POST', body: JSON.stringify({username: $('username').value.trim(), password: $('password').value})});
    await bootstrap();
  } catch (error) { els.loginError.textContent = error.code === 'AUTH_INVALID' ? '用户名或密码错误' : error.message; }
  finally { els.loginButton.disabled = false; }
}
async function logout() {
  try { await api('/api/v1/auth/logout', {method: 'POST'}); } catch {}
  stopPolling(); closeRunStream(); closeConversationStream(); localStorage.removeItem('workspace.activeRun'); localStorage.removeItem('workspace.projectId');
  state.user = null; state.projects = []; state.project = null; state.role = null; state.members = []; state.projectAgents = [];
  state.ownedAgents = []; state.conversations = []; state.messages = []; state.activeConversation = null; state.run = null;
  state.conversationEventCursor = 0; state.lastRunId = null; state.lastRunOwnerId = null; showAuth();
}
async function bootstrap() {
  try {
    state.user = await api('/api/v1/me'); els.userName.textContent = state.user.display_name;
    els.userAvatar.textContent = (state.user.display_name || 'U').slice(0, 1).toUpperCase();
    showWorkspace(); await loadWorkspace(); await restoreActiveRun();
  } catch (error) { if (error.status === 401) { showAuth(); return; } throw error; }
}

async function loadWorkspace() {
  const [projectsResponse, agentsResponse] = await Promise.all([api('/api/v1/projects'), api('/api/v1/agents')]);
  state.projects = projectsResponse.items || []; state.ownedAgents = agentsResponse.items || []; renderProjectSelect();
  if (!state.projects.length) { resetProjectSelection(); updateEmptyState(); applyPermissions(); return; }
  const remembered = localStorage.getItem('workspace.projectId');
  const projectId = state.projects.some((project) => project.id === remembered) ? remembered : state.projects[0].id;
  await selectProject(projectId);
}
function renderProjectSelect() {
  els.projectSelect.replaceChildren();
  for (const project of state.projects) { const option = document.createElement('option'); option.value = project.id; option.textContent = project.name; els.projectSelect.append(option); }
  if (state.project) els.projectSelect.value = state.project.id;
}
function resetProjectSelection() {
  closeConversationStream();
  state.project = null; state.role = null; state.members = []; state.projectAgents = []; state.conversations = []; state.messages = [];
  state.activeConversation = null; state.targetAgentId = null; els.projectName.textContent = '未选择 Project';
  els.projectDescription.textContent = '创建或加入 Project 后开始协作'; els.roleBadge.textContent = '—'; els.roleBadge.className = 'role-badge';
  els.userRole.textContent = '未加入 Project'; renderConversations(); renderAgents(); renderMembers(); renderProjectAgents(); clearConversationView(); applyPermissions();
}
async function selectProject(projectId) {
  if (!projectId) return;
  closeConversationStream();
  const encoded = encodeURIComponent(projectId);
  const [project, members, agents, conversations] = await Promise.all([
    api(`/api/v1/projects/${encoded}`), api(`/api/v1/projects/${encoded}/members`),
    api(`/api/v1/projects/${encoded}/agents`), api(`/api/v1/projects/${encoded}/conversations`)
  ]);
  state.project = project; state.members = members.items || []; state.projectAgents = agents.items || []; state.conversations = conversations.items || [];
  state.role = currentMember()?.role || null; localStorage.setItem('workspace.projectId', project.id);
  renderProjectSelect(); renderProjectHeader(); renderMembers(); renderProjectAgents(); renderAgents(); renderConversations(); applyPermissions(); startPolling();
  const rememberedConversation = localStorage.getItem(`workspace.conversation.${project.id}`);
  const conversation = state.conversations.find((item) => item.id === rememberedConversation) || state.conversations[0];
  if (conversation) await selectConversation(conversation.id); else clearConversationView();
}

function renderProjectHeader() {
  els.projectName.textContent = state.project?.name || '未选择 Project';
  els.projectDescription.textContent = state.project?.description || 'Project 成员共享成员、Agent 与会话';
  els.roleBadge.textContent = state.role || '非成员'; els.roleBadge.className = `role-badge ${state.role || ''}`.trim();
  els.userRole.textContent = state.role ? `${state.role} · Project 成员` : '未加入 Project';
  els.settingsProjectName.textContent = state.project?.name || 'Project 详情';
}
function renderMembers() {
  els.memberList.replaceChildren();
  for (const member of state.members) {
    const row = document.createElement('div'); row.className = 'settings-row';
    const main = document.createElement('div'); main.className = 'settings-row-main';
    const name = document.createElement('strong'); name.textContent = member.display_name || member.username || member.user_id;
    const detail = document.createElement('small'); detail.textContent = `${member.role} · ${member.user_id}`; main.append(name, detail);
    const actions = document.createElement('div'); actions.className = 'settings-row-actions';
    if (canManage()) {
      const role = document.createElement('select'); role.setAttribute('aria-label', `修改 ${name.textContent} 的角色`);
      for (const value of ['owner', 'editor', 'viewer']) { const option = document.createElement('option'); option.value = value; option.textContent = value; option.selected = value === member.role; role.append(option); }
      role.addEventListener('change', () => updateMemberRole(member.user_id, role.value));
      const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'text-button'; remove.textContent = '移除'; remove.addEventListener('click', () => removeMember(member.user_id));
      actions.append(role, remove);
    } else { const badge = document.createElement('span'); badge.className = `role-badge ${member.role}`; badge.textContent = member.role; actions.append(badge); }
    row.append(main, actions); els.memberList.append(row);
  }
}
function renderProjectAgents() {
  els.projectAgentList.replaceChildren();
  for (const item of state.projectAgents) {
    const row = document.createElement('div'); row.className = 'settings-row';
    const main = document.createElement('div'); main.className = 'settings-row-main';
    const name = document.createElement('strong'); name.textContent = item.agent?.name || item.agent_id;
    const detail = document.createElement('small'); detail.textContent = `${item.agent?.model_provider?.model_name || 'mock'} · ${item.agent_id}`; main.append(name, detail);
    const actions = document.createElement('div'); actions.className = 'settings-row-actions';
    if (canManage() || (canWrite() && item.added_by_user_id === state.user?.id)) {
      const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'text-button'; remove.textContent = '移出 Project'; remove.addEventListener('click', () => removeProjectAgent(item.agent_id)); actions.append(remove);
    }
    row.append(main, actions); els.projectAgentList.append(row);
  }
  renderOwnedAgentOptions();
}
function renderOwnedAgentOptions() {
  els.ownedAgentSelect.replaceChildren();
  const projectIds = new Set(state.projectAgents.map((item) => item.agent_id));
  const available = state.ownedAgents.filter((agent) => agent.status !== 'disabled' && !projectIds.has(agent.id));
  if (!available.length) {
    const option = document.createElement('option'); option.value = ''; option.textContent = '没有可加入的 Agent'; els.ownedAgentSelect.append(option);
    els.ownedAgentSelect.disabled = true; els.addOwnedAgent.disabled = true; return;
  }
  for (const agent of available) { const option = document.createElement('option'); option.value = agent.id; option.textContent = agent.name; els.ownedAgentSelect.append(option); }
  els.ownedAgentSelect.disabled = false; els.addOwnedAgent.disabled = false;
}
function renderAgents() {
  els.agentSelect.replaceChildren();
  for (const item of state.projectAgents) { const option = document.createElement('option'); option.value = item.agent_id; option.textContent = item.agent?.name || item.agent_id; els.agentSelect.append(option); }
  const remembered = state.project ? localStorage.getItem(`workspace.targetAgent.${state.project.id}`) : null;
  const valid = state.projectAgents.some((item) => item.agent_id === remembered);
  state.targetAgentId = valid ? remembered : state.projectAgents[0]?.agent_id || null;
  if (state.targetAgentId) els.agentSelect.value = state.targetAgentId;
  applyPermissions();
}
function renderConversations() {
  els.conversationList.replaceChildren(); els.conversationCount.textContent = String(state.conversations.length);
  for (const conversation of state.conversations) {
    const button = document.createElement('button'); button.type = 'button'; button.className = 'conversation-item';
    if (state.activeConversation?.id === conversation.id) button.classList.add('active');
    const title = document.createElement('strong'); title.textContent = conversation.title;
    const meta = document.createElement('span'); meta.textContent = `默认 ${conversation.agent_name || 'Agent'} · ${formatTime(conversation.updated_at)}`;
    button.append(title, meta); button.addEventListener('click', () => selectConversation(conversation.id)); els.conversationList.append(button);
  }
}
function updateEmptyState() {
  els.emptyState.classList.remove('hidden'); els.messages.classList.add('hidden');
  const heading = els.emptyState.querySelector('h3'); const copy = els.emptyState.querySelector('p');
  if (!state.project) { heading.textContent = '先创建一个 Project'; copy.textContent = 'Project 是成员、Agent、共享会话与权限的统一边界。'; els.emptyAction.textContent = '创建 Project'; }
  else if (!state.projectAgents.length) { heading.textContent = '向 Project 加入 Agent'; copy.textContent = 'owner/editor 可以在 Project 详情中加入自己拥有的 Agent。'; els.emptyAction.textContent = '管理 Project'; }
  else if (!canWrite()) { heading.textContent = '当前为只读成员'; copy.textContent = 'viewer 可以查看共享历史与 SSE 运行状态，但不能发送、取消或重试。'; els.emptyAction.textContent = '查看 Project 详情'; }
  else if (state.activeConversation) { heading.textContent = '这个共享会话还没有消息'; copy.textContent = '选择 Project Agent 发送第一条消息，其他成员会看到同一份历史与流式回复。'; els.emptyAction.textContent = '发送消息'; }
  else if (!state.conversations.length) { heading.textContent = '创建第一个共享会话'; copy.textContent = 'Project 成员会看到同一份消息历史，并可选择不同 Agent 回复。'; els.emptyAction.textContent = '创建共享会话'; }
  else { heading.textContent = '选择一个共享会话'; copy.textContent = '与会成员共享完整历史，运行事件通过既有 SSE 序列发布。'; els.emptyAction.textContent = '创建共享会话'; }
  els.emptyAction.disabled = !state.project && !state.user; els.emptyAction.classList.toggle('hidden', Boolean(state.activeConversation)); applyPermissions();
}
function clearConversationView() {
  state.activeConversation = null; state.messages = []; els.messages.replaceChildren();
  els.conversationTitle.textContent = state.project ? '选择一个共享会话' : '选择一个 Project';
  els.conversationSubtitle.textContent = 'Project 成员共享完整消息历史'; updateEmptyState();
}
async function selectConversation(id) {
  const conversation = state.conversations.find((item) => item.id === id); if (!conversation) return;
  document.querySelector?.('.sidebar')?.classList.remove('open');
  state.activeConversation = conversation; localStorage.setItem(`workspace.conversation.${state.project.id}`, conversation.id);
  renderConversations(); els.emptyState.classList.add('hidden'); els.messages.classList.remove('hidden');
  els.conversationTitle.textContent = conversation.title; els.conversationSubtitle.textContent = `${state.project.name} · ${state.role} · ${state.projectAgents.length} 个 Project Agent`;
  clearError(); connectConversationStream(); await loadMessages(true); if (state.run?.conversationId === id && !state.eventSource) connectRun(); startPolling();
}
function messageSenderName(message) {
  if (message.sender_type === 'agent') { const projectAgent = state.projectAgents.find((item) => item.agent_id === message.sender_id); return projectAgent?.agent?.name || state.activeConversation?.agent_name || 'Agent'; }
  if (message.sender_id === state.user?.id) return state.user.display_name || '你';
  const member = state.members.find((item) => item.user_id === message.sender_id); return member?.display_name || member?.username || '协作者';
}
function buildMessage(message, streaming = false) {
  const article = document.createElement('article'); article.className = `message ${message.sender_type || 'agent'}${streaming ? ' streaming' : ''}`;
  if (message.id) article.dataset.messageId = message.id;
  const avatar = document.createElement('div'); avatar.className = 'message-avatar'; avatar.textContent = message.sender_type === 'user' ? 'YOU' : 'AI';
  const body = document.createElement('div'); body.className = 'message-body'; const meta = document.createElement('div'); meta.className = 'message-meta';
  const sender = document.createElement('strong'); sender.textContent = messageSenderName(message); const time = document.createElement('span'); time.textContent = formatTime(message.created_at) || '刚刚'; meta.append(sender, time);
  const content = document.createElement('div'); content.className = 'message-content'; content.textContent = message.content || ''; body.append(meta, content); article.append(avatar, body); return article;
}
function renderMessages() {
  updateEmptyState(); els.messages.replaceChildren(); for (const message of state.messages) els.messages.append(buildMessage(message));
  if (state.run?.conversationId === state.activeConversation?.id) { const pending = buildMessage({sender_type: 'agent', sender_id: state.run.targetAgentId, content: state.run.partial || ''}, true); pending.id = 'streaming-message'; els.messages.append(pending); }
  const showEmpty = state.messages.length === 0 && !state.run; els.emptyState.classList.toggle('hidden', !showEmpty); els.messages.classList.toggle('hidden', showEmpty); scrollToBottom();
}
function scrollToBottom() { requestAnimationFrame(() => { const stage = $('message-stage'); stage.scrollTop = stage.scrollHeight; }); }

async function loadMessages(showError = true) {
  const conversationId = state.activeConversation?.id; if (!conversationId) return false;
  const loadSeq = ++state.messageLoadSeq;
  try {
    const response = await api(`/api/v1/conversations/${encodeURIComponent(conversationId)}/messages`);
    if (loadSeq !== state.messageLoadSeq || state.activeConversation?.id !== conversationId) return false;
    state.messages = response.items || []; const discovered = await discoverRunFromMessages(); renderMessages(); return discovered;
  } catch (error) { if (loadSeq === state.messageLoadSeq && showError && state.activeConversation?.id === conversationId) setError(error.message, error.retryable); return false; }
}
async function discoverRunFromMessages() {
  if (state.run || !state.activeConversation) return false;
  const conversationId = state.activeConversation.id;
  const completed = new Set(state.messages.filter((message) => message.sender_type === 'agent' && message.run_id).map((message) => message.run_id));
  const pending = [...state.messages].reverse().find((message) => message.sender_type === 'user' && message.run_id && !completed.has(message.run_id) && !state.terminalRuns.has(message.run_id));
  if (!pending) return false;
  try {
    const run = await api(`/api/v1/runs/${encodeURIComponent(pending.run_id)}`);
    if (state.run || state.activeConversation?.id !== conversationId) return false;
    if (!['queued', 'running'].includes(run.status)) { state.terminalRuns.add(pending.run_id); return false; }
    beginRun(pending.run_id, conversationId, 0, '', run.initiated_by_user_id, run.agent_id); return true;
  } catch { return false; }
}
function persistRun() {
  if (!state.run) { localStorage.removeItem('workspace.activeRun'); return; }
  localStorage.setItem('workspace.activeRun', JSON.stringify({runId: state.run.runId, conversationId: state.run.conversationId,
    projectId: state.project?.id || null, ownerId: state.run.ownerId || null, targetAgentId: state.run.targetAgentId || null,
    lastEventId: state.run.lastEventId, partial: state.run.partial}));
}
function closeRunStream() { if (state.eventSource) { state.eventSource.close(); state.eventSource = null; } }
function conversationCursorKey(conversationId) { return `workspace.conversationEventCursor.${conversationId}`; }
function closeConversationStream() { if (state.conversationEventSource) { state.conversationEventSource.close(); state.conversationEventSource = null; } }
function connectConversationStream() {
  if (!state.activeConversation) return;
  closeConversationStream();
  const conversation = state.activeConversation;
  const savedCursor = Number(localStorage.getItem(conversationCursorKey(conversation.id))) || 0;
  state.conversationEventCursor = savedCursor;
  const requestId = `web-conversation-sse-${uuid()}`;
  const after = savedCursor > 0 ? `&after=${savedCursor}` : '';
  const source = new EventSource(`/api/v1/conversations/${encodeURIComponent(conversation.id)}/events?request_id=${encodeURIComponent(requestId)}${after}`);
  state.conversationEventSource = source;
  source.addEventListener('run.available', (event) => handleConversationEvent(event, conversation.id));
  source.onerror = () => {
    if (state.activeConversation?.id !== conversation.id || state.conversationEventSource !== source) return;
    if (source.readyState === 2) {
      source.close(); state.conversationEventSource = null;
      setTimeout(() => { if (state.activeConversation?.id === conversation.id && !state.conversationEventSource) connectConversationStream(); }, 1800);
    }
  };
}
function handleConversationEvent(event, conversationId) {
  if (state.activeConversation?.id !== conversationId) return;
  let envelope; try { envelope = JSON.parse(event.data); } catch { return; }
  const seq = Number(envelope.seq) || 0;
  if (seq && seq <= state.conversationEventCursor) return;
  if (seq) {
    state.conversationEventCursor = seq;
    localStorage.setItem(conversationCursorKey(conversationId), String(seq));
  }
  const payload = envelope.payload || {};
  if (envelope.type !== 'run.available' || !payload.run_id || !payload.target_agent_id || !payload.initiated_by_user_id) return;
  if (!['queued', 'running'].includes(payload.status) || state.terminalRuns.has(payload.run_id)) return;
  if (state.messages.some((message) => message.sender_type === 'agent' && message.run_id === payload.run_id)) {
    state.terminalRuns.add(payload.run_id); return;
  }
  if (state.run?.runId === payload.run_id) {
    state.run.targetAgentId = payload.target_agent_id;
    state.run.ownerId = payload.initiated_by_user_id;
    persistRun(); renderMessages(); applyPermissions(); return;
  }
  if (state.run?.conversationId === conversationId) return;
  beginRun(payload.run_id, conversationId, 0, '', payload.initiated_by_user_id, payload.target_agent_id);
}

async function sendMessage(event) {
  event?.preventDefault(); const content = els.messageInput.value.trim();
  if (!content || !state.activeConversation || state.run || !canWrite() || !state.targetAgentId) return;
  clearError(); setSending(true); setRunState('active', '正在创建 Run'); const requestId = `web-${uuid()}`;
  try {
    const response = await api(`/api/v1/conversations/${encodeURIComponent(state.activeConversation.id)}/messages`, {
      method: 'POST', requestId, headers: {'Idempotency-Key': requestId},
      body: JSON.stringify({content, client_request_id: requestId, target_agent_id: state.targetAgentId})
    });
    els.messageInput.value = ''; els.messageInput.style.height = 'auto';
    if (!state.messages.some((message) => message.id === response.message.id)) state.messages.push(response.message);
    renderMessages(); state.lastRunOwnerId = state.user.id;
    beginRun(response.run_id, state.activeConversation.id, 0, '', state.user.id, state.targetAgentId);
  } catch (error) {
    state.sending = false; setRunState('error', error.status === 409 ? '运行冲突' : '创建失败');
    setError(error.status === 409 ? '当前共享会话已有运行中的任务，请等待完成后再发送。' : error.message, error.retryable);
    await loadMessages(false);
  } finally { if (!state.run) setSending(false); }
}
async function createConversation() {
  if (!state.project) { openProjectDialog(); return; }
  if (!canWrite()) { toast('viewer 不能创建共享会话'); return; }
  if (!state.projectAgents.length || !state.targetAgentId) { openProjectSettings(); return; }
  try {
    const conversation = await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/conversations`, {
      method: 'POST', body: JSON.stringify({agent_id: state.targetAgentId, title: `共享对话 ${new Date().toLocaleDateString('zh-CN')}`})
    });
    state.conversations.unshift(conversation); renderConversations(); await selectConversation(conversation.id); toast('共享会话已创建');
  } catch (error) { toast(error.message); }
}
async function createAgent(event) {
  event.preventDefault(); els.agentError.textContent = ''; els.agentSubmit.disabled = true;
  try {
    const agent = await api('/api/v1/agents', {method: 'POST', body: JSON.stringify({name: els.agentName.value.trim(), system_prompt: els.agentPrompt.value.trim()})});
    state.ownedAgents.unshift(agent);
    if (state.project && canWrite()) {
      await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/agents`, {method: 'POST', body: JSON.stringify({agent_id: agent.id})});
      await refreshProjectData(); state.targetAgentId = agent.id; localStorage.setItem(`workspace.targetAgent.${state.project.id}`, agent.id); renderAgents(); updateEmptyState();
    }
    els.agentDialog.close(); toast(state.project && canWrite() ? 'Agent 已创建并加入 Project' : 'Agent 已创建');
  } catch (error) { els.agentError.textContent = error.message; } finally { els.agentSubmit.disabled = false; }
}
async function cancelRun() {
  if (!state.run || !canCancelRun()) return; state.cancelling = true; applyPermissions(); setRunState('active', '正在取消');
  try { await api(`/api/v1/runs/${encodeURIComponent(state.run.runId)}/cancel`, {method: 'POST'}); }
  catch (error) { setError(error.message, false); } finally { state.cancelling = false; applyPermissions(); }
}
async function retryRun() {
  if (!state.lastRunId || state.retrying || !canRetryLast()) return;
  state.retrying = true; els.retryButton.disabled = true; clearError(); setRunState('active', '正在重试'); setSending(true);
  try {
    const response = await api(`/api/v1/runs/${encodeURIComponent(state.lastRunId)}/retry`, {method: 'POST', headers: {'Idempotency-Key': `retry-${uuid()}`}});
    if (!state.messages.some((message) => message.id === response.message.id)) state.messages.push(response.message);
    renderMessages(); state.lastRunOwnerId = state.user.id;
    beginRun(response.run_id, state.activeConversation.id, 0, '', state.user.id, response.message?.target_agent_id || response.target_agent_id || state.targetAgentId);
  } catch (error) { state.sending = false; setRunState('error', '重试失败'); setError(error.message, error.retryable); }
  finally { state.retrying = false; if (!state.run) setSending(false); }
}
function beginRun(runId, conversationId, lastEventId = 0, partial = '', ownerId = null, targetAgentId = null) {
  const existing = state.run?.runId === runId ? state.run : null;
  if (!existing) closeRunStream();
  state.run = {
    runId, conversationId,
    ownerId: existing?.ownerId || ownerId || null,
    targetAgentId: existing?.targetAgentId || targetAgentId || null,
    lastEventId: existing ? existing.lastEventId : Number(lastEventId) || 0,
    partial: existing ? existing.partial : partial || '',
    reconnecting: false,
  };
  state.lastRunId = runId; state.lastRunOwnerId = state.run.ownerId; state.sending = false; persistRun();
  setRunState('active', state.run.ownerId && state.run.ownerId !== state.user?.id ? '协作者运行中' : 'Run 已排队'); clearError();
  if (state.activeConversation?.id === conversationId) {
    renderMessages(); applyPermissions();
    if (!state.eventSource) connectRun();
  }
}
function connectRun() {
  if (!state.run) return; closeRunStream(); const run = state.run; const requestId = `web-sse-${uuid()}`;
  const source = new EventSource(`/api/v1/runs/${encodeURIComponent(run.runId)}/events?request_id=${encodeURIComponent(requestId)}&after=${run.lastEventId}`); state.eventSource = source;
  source.onopen = () => { if (state.run?.runId === run.runId && state.eventSource === source) { state.run.reconnecting = false; setRunState('active', run.ownerId && run.ownerId !== state.user?.id ? '协作者运行中' : '流式接收中'); } };
  for (const name of ['run.started', 'message.delta', 'message.completed', 'run.failed', 'run.cancelled']) source.addEventListener(name, (event) => handleRunEvent(name, event, run.runId));
  source.onerror = () => {
    if (!state.run || state.run.runId !== run.runId || state.eventSource !== source) return;
    if (source.readyState === 0) { state.run.reconnecting = true; setRunState('active', '连接中断，正在按 Last-Event-ID 回放'); }
    else if (source.readyState === 2) { source.close(); if (state.eventSource === source) state.eventSource = null; setRunState('error', '事件连接失败'); setError('SSE 连接失败，将使用已保存游标重连', true); setTimeout(() => { if (state.run?.runId === run.runId && !state.eventSource) connectRun(); }, 1800); }
  };
}
function handleRunEvent(name, event, runId) {
  if (!state.run || state.run.runId !== runId) return; let envelope; try { envelope = JSON.parse(event.data); } catch { return; }
  const seq = Number(envelope.seq) || 0; if (seq && seq <= state.run.lastEventId) return;
  state.run.lastEventId = seq || state.run.lastEventId;
  if (name === 'run.started') setRunState('active', state.run.ownerId && state.run.ownerId !== state.user?.id ? '协作者运行中' : 'Worker 执行中');
  if (name === 'message.delta') { state.run.partial += envelope.payload?.delta || ''; updateStreamingNode(); }
  if (name === 'message.completed') { state.run.partial = envelope.payload?.content || state.run.partial; updateStreamingNode(); }
  persistRun();
  if (name === 'message.completed') finishRun('succeeded', envelope);
  if (name === 'run.failed') finishRun('failed', envelope);
  if (name === 'run.cancelled') finishRun('cancelled', envelope);
}
function updateStreamingNode() {
  let node = $('streaming-message'); if (!node) { renderMessages(); node = $('streaming-message'); }
  if (node) { const content = node.querySelector('.message-content'); if (content) content.textContent = state.run?.partial || ''; scrollToBottom(); }
}

async function finishRun(kind, envelope) {
  if (!state.run) return; const conversationId = state.run.conversationId; const finishedRunId = state.run.runId;
  state.terminalRuns.add(finishedRunId); closeRunStream(); state.run = null; state.lastRunId = finishedRunId;
  localStorage.removeItem('workspace.activeRun'); setSending(false);
  if (state.activeConversation?.id === conversationId) {
    if (kind === 'succeeded') { setRunState('success', '已完成'); clearError(); }
    if (kind === 'failed') { const code = envelope.payload?.error_code || 'MODEL_PROVIDER_ERROR'; setRunState('error', '运行失败'); setError(`运行失败（${code}）。可以重新运行。`, true); }
    if (kind === 'cancelled') { setRunState('error', '已取消'); setError('运行已取消。可以重新运行。', true); }
    await loadMessages(false);
  }
  const conversation = state.conversations.find((item) => item.id === conversationId);
  if (conversation) { conversation.updated_at = new Date().toISOString(); renderConversations(); }
  applyPermissions();
}
async function restoreActiveRun() {
  const raw = localStorage.getItem('workspace.activeRun'); if (!raw) return; let saved;
  try { saved = JSON.parse(raw); } catch { localStorage.removeItem('workspace.activeRun'); return; }
  if (saved.projectId && state.project?.id !== saved.projectId && state.projects.some((project) => project.id === saved.projectId)) await selectProject(saved.projectId);
  const conversation = state.conversations.find((item) => item.id === saved.conversationId);
  if (!conversation) { localStorage.removeItem('workspace.activeRun'); return; }
  if (state.activeConversation?.id !== conversation.id) await selectConversation(conversation.id);
  const finalMessage = state.messages.find((message) => message.sender_type === 'agent' && message.run_id === saved.runId);
  if (finalMessage) { localStorage.removeItem('workspace.activeRun'); state.lastRunId = saved.runId; state.lastRunOwnerId = saved.ownerId || null; state.terminalRuns.add(saved.runId); setRunState('success', '已完成'); applyPermissions(); return; }
  state.lastRunId = saved.runId; state.lastRunOwnerId = saved.ownerId || null;
  beginRun(saved.runId, saved.conversationId, saved.lastEventId || 0, saved.partial || '', saved.ownerId || null, saved.targetAgentId || null);
}
function startPolling() {
  stopPolling(); state.pollTimer = setInterval(async () => { if (!state.activeConversation) return; await loadMessages(false); }, 2500);
}
function stopPolling() { if (state.pollTimer) clearInterval(state.pollTimer); state.pollTimer = null; }

function openProjectDialog() {
  els.projectError.textContent = ''; els.projectNameInput.value = ''; els.projectDescriptionInput.value = '';
  els.projectCreateDialog.showModal(); setTimeout(() => els.projectNameInput.focus(), 30);
}
async function createProject(event) {
  event.preventDefault(); els.projectError.textContent = ''; els.projectSubmit.disabled = true;
  try {
    const project = await api('/api/v1/projects', {method: 'POST', body: JSON.stringify({name: els.projectNameInput.value.trim(), description: els.projectDescriptionInput.value.trim() || null})});
    state.projects.unshift(project); renderProjectSelect(); els.projectCreateDialog.close(); await selectProject(project.id); toast('Project 已创建');
  } catch (error) { els.projectError.textContent = error.message; } finally { els.projectSubmit.disabled = false; }
}
function openProjectSettings() {
  if (!state.project) return; els.settingsError.textContent = ''; els.memberForm.classList.add('hidden');
  renderMembers(); renderProjectAgents(); els.projectSettingsDialog.showModal();
}
async function refreshProjectData() {
  if (!state.project) return; const encoded = encodeURIComponent(state.project.id);
  const [members, agents, conversations] = await Promise.all([api(`/api/v1/projects/${encoded}/members`), api(`/api/v1/projects/${encoded}/agents`), api(`/api/v1/projects/${encoded}/conversations`)]);
  state.members = members.items || []; state.projectAgents = agents.items || []; state.conversations = conversations.items || []; state.role = currentMember()?.role || null;
  renderProjectHeader(); renderMembers(); renderProjectAgents(); renderAgents(); renderConversations(); applyPermissions();
}
async function addMember(event) {
  event.preventDefault(); els.settingsError.textContent = '';
  try {
    await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/members`, {method: 'POST', body: JSON.stringify({user_id: els.memberUserId.value.trim(), role: els.memberRole.value})});
    els.memberUserId.value = ''; await refreshProjectData(); toast('成员已添加');
  } catch (error) { els.settingsError.textContent = error.message; }
}
async function updateMemberRole(userId, role) {
  els.settingsError.textContent = '';
  try { await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/members/${encodeURIComponent(userId)}`, {method: 'PATCH', body: JSON.stringify({role})}); await refreshProjectData(); toast('成员角色已更新'); }
  catch (error) { els.settingsError.textContent = error.message; await refreshProjectData(); }
}
async function removeMember(userId) {
  if (!canManage()) return; els.settingsError.textContent = '';
  try { await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/members/${encodeURIComponent(userId)}`, {method: 'DELETE'}); await refreshProjectData(); toast('成员已移除'); }
  catch (error) { els.settingsError.textContent = error.message; }
}
async function addOwnedAgent() {
  const agentId = els.ownedAgentSelect.value; if (!agentId || !canWrite()) return; els.settingsError.textContent = '';
  try {
    await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/agents`, {method: 'POST', body: JSON.stringify({agent_id: agentId})});
    await refreshProjectData(); state.targetAgentId = agentId; localStorage.setItem(`workspace.targetAgent.${state.project.id}`, agentId); renderAgents(); toast('Agent 已加入 Project');
  } catch (error) { els.settingsError.textContent = error.message; }
}
async function removeProjectAgent(agentId) {
  els.settingsError.textContent = '';
  try { await api(`/api/v1/projects/${encodeURIComponent(state.project.id)}/agents/${encodeURIComponent(agentId)}`, {method: 'DELETE'}); await refreshProjectData(); toast('Agent 已移出 Project'); }
  catch (error) { els.settingsError.textContent = error.message; }
}

function showAgentDialog() { els.agentError.textContent = ''; els.agentName.value = ''; els.agentPrompt.value = ''; els.agentDialog.showModal(); setTimeout(() => els.agentName.focus(), 30); }
function autoResize() { els.messageInput.style.height = 'auto'; els.messageInput.style.height = `${Math.min(els.messageInput.scrollHeight, 180)}px`; }
function bindEvents() {
  els.loginForm.addEventListener('submit', login); els.logout.addEventListener('click', logout); els.messageForm.addEventListener('submit', sendMessage);
  els.newConversation.addEventListener('click', createConversation); els.newAgent.addEventListener('click', showAgentDialog);
  els.newProject.addEventListener('click', openProjectDialog); els.projectSettings.addEventListener('click', openProjectSettings);
  els.projectSelect.addEventListener('change', () => selectProject(els.projectSelect.value)); els.projectForm.addEventListener('submit', createProject);
  els.agentForm.addEventListener('submit', createAgent); els.memberForm.addEventListener('submit', addMember);
  els.showMemberForm.addEventListener('click', () => els.memberForm.classList.toggle('hidden')); els.addOwnedAgent.addEventListener('click', addOwnedAgent);
  els.cancelButton.addEventListener('click', cancelRun); els.retryButton.addEventListener('click', retryRun);
  els.emptyAction.addEventListener('click', () => { if (!state.project) openProjectDialog(); else if (!state.projectAgents.length || !canWrite()) openProjectSettings(); else createConversation(); });
  els.agentSelect.addEventListener('change', () => { state.targetAgentId = els.agentSelect.value; if (state.project) localStorage.setItem(`workspace.targetAgent.${state.project.id}`, state.targetAgentId); applyPermissions(); });
  els.messageInput.addEventListener('input', autoResize);
  els.messageInput.addEventListener('keydown', (event) => { if (event.key === 'Enter' && !event.shiftKey && !event.isComposing) { event.preventDefault(); els.messageForm.requestSubmit(); } });
  document.querySelectorAll('[data-close-dialog]').forEach((button) => { button.addEventListener('click', () => $(button.dataset.closeDialog)?.close()); });
  const sidebarToggle = $('sidebar-toggle'); if (sidebarToggle) sidebarToggle.addEventListener('click', () => document.querySelector('.sidebar').classList.toggle('open'));
  document.addEventListener?.('visibilitychange', () => { if (!document.hidden && state.activeConversation) loadMessages(false); });
}
window.addEventListener('DOMContentLoaded', async () => {
  bindEvents();
  try { await bootstrap(); } catch (error) { showAuth(); els.loginError.textContent = error.message || '服务启动中，请稍后重试'; }
});
