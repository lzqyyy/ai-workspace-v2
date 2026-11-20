'use strict';

const assert = require('assert');
const fs = require('fs');
const vm = require('vm');

function element(id = '') {
  const classes = new Set();
  const node = {
    id,
    disabled: false,
    hidden: false,
    dataset: {},
    textContent: '',
    value: '',
    style: {},
    classList: {
      add(...names) { names.forEach((name) => classes.add(name)); node.className = [...classes].join(' '); },
      remove(...names) { names.forEach((name) => classes.delete(name)); node.className = [...classes].join(' '); },
      toggle(name, force) {
        const enabled = force === undefined ? !classes.has(name) : Boolean(force);
        if (enabled) classes.add(name); else classes.delete(name);
        node.className = [...classes].join(' ');
        return enabled;
      },
      contains(name) { return classes.has(name); },
    },
    addEventListener() {},
    append() {},
    replaceChildren() {},
    querySelector() { return element(); },
    requestSubmit() {},
    focus() {},
    showModal() {},
    close() {},
  };
  return node;
}

const elements = new Map();
const emptyHeading = element('empty-heading');
const emptyCopy = element('empty-copy');
const sidebarClasses = new Set();
const sidebar = {
  classList: {
    add(value) { sidebarClasses.add(value); },
    remove(value) { sidebarClasses.delete(value); },
    toggle(value) { sidebarClasses.has(value) ? sidebarClasses.delete(value) : sidebarClasses.add(value); },
    contains(value) { return sidebarClasses.has(value); },
  },
};
const document = {
  getElementById(id) {
    if (!elements.has(id)) elements.set(id, element(id));
    return elements.get(id);
  },
  querySelectorAll() { return []; },
  querySelector(selector) { return selector === '.sidebar' ? sidebar : null; },
  addEventListener() {},
  hidden: false,
};
document.getElementById('empty-state').querySelector = (selector) => selector === 'h3' ? emptyHeading : emptyCopy;

const storage = new Map();
const localStorage = {
  getItem(key) { return storage.has(key) ? storage.get(key) : null; },
  setItem(key, value) { storage.set(key, String(value)); },
  removeItem(key) { storage.delete(key); },
};
let uuidCounter = 0;
const context = {
  console,
  document,
  localStorage,
  Headers,
  crypto: { randomUUID: () => `00000000-0000-4000-8000-${String(++uuidCounter).padStart(12, '0')}` },
  window: { addEventListener() {} },
  setTimeout,
  clearTimeout,
  setInterval: () => 1,
  clearInterval() {},
  requestAnimationFrame: (callback) => callback(),
  Intl,
  Promise,
  Date,
  Number,
  Boolean,
  JSON,
  Set,
  encodeURIComponent,
  EventSource: class { constructor() {} addEventListener() {} close() {} },
};
vm.createContext(context);
vm.runInContext(fs.readFileSync('frontend/app.js', 'utf8'), context);
context.__originalSelectConversation = vm.runInContext('selectConversation', context);

(async () => {
  vm.runInContext(`
    state.user = {id:'user-a', display_name:'User A'};
    state.project = {id:'project-1', name:'Project One'};
    state.members = [
      {user_id:'user-a', display_name:'User A', role:'viewer'},
      {user_id:'user-b', display_name:'User B', role:'editor'}
    ];
    state.projectAgents = [{agent_id:'agent-1', added_by_user_id:'user-a', agent:{name:'Agent 1', status:'active'}}];
    state.activeConversation = {id:'conversation-1', title:'Shared'};
    state.role = 'viewer';
    applyPermissions();
  `, context);
  assert.strictEqual(elements.get('message-input').disabled, true, 'viewer input must be disabled');
  assert.strictEqual(elements.get('send-button').disabled, true, 'viewer send must be hidden by disabled state');
  assert.strictEqual(elements.get('new-conversation').disabled, true, 'viewer cannot create conversations');
  assert.strictEqual(elements.get('readonly-note').classList.contains('hidden'), false, 'viewer must see readonly note');
  assert.strictEqual(elements.get('show-member-form').classList.contains('hidden'), true, 'viewer cannot manage members');

  vm.runInContext(`
    state.role = 'editor'; state.run = {runId:'other-run', conversationId:'conversation-1', ownerId:'user-b', targetAgentId:'agent-1', lastEventId:0, partial:''};
    applyPermissions();
  `, context);
  assert.strictEqual(elements.get('message-input').disabled, true, 'active run must lock editor input');
  assert.strictEqual(elements.get('cancel-button').classList.contains('hidden'), true, 'editor cannot cancel another member run');
  vm.runInContext("state.role = 'owner'; applyPermissions();", context);
  assert.strictEqual(elements.get('cancel-button').classList.contains('hidden'), false, 'owner can cancel any run');
  assert.strictEqual(elements.get('show-member-form').classList.contains('hidden'), false, 'owner can manage members');

  vm.runInContext(`
    state.role = 'editor'; state.run = null; state.sending = false; state.targetAgentId = 'agent-1';
    state.activeConversation = {id:'conversation-1'}; state.messages = [];
    els.messageInput.value = 'hello shared conversation'; els.messageInput.style = {};
    beginRun = (...args) => { __beginArgs = args; };
    renderMessages = () => {};
    api = async (path, options) => {
      __sendCapture = {path, options};
      return {message:{id:'message-1', sender_type:'user', content:'hello shared conversation'}, run_id:'run-1'};
    };
    __beginArgs = null;
  `, context);
  await context.sendMessage({preventDefault() {}});
  const sendCapture = context.__sendCapture;
  assert.strictEqual(sendCapture.path, '/api/v1/conversations/conversation-1/messages');
  const sendBody = JSON.parse(sendCapture.options.body);
  assert.strictEqual(sendBody.target_agent_id, 'agent-1', 'send must include target_agent_id');
  assert.ok(sendBody.client_request_id, 'send must include client_request_id');
  assert.strictEqual(context.__beginArgs[0], 'run-1', 'send must start returned run');

  vm.runInContext(`
    state.project = {id:'project-1'}; state.targetAgentId = 'agent-1'; state.conversations = [];
    selectConversation = async () => {};
    toast = () => {};
    api = async (path, options) => {
      __conversationCapture = {path, options};
      return {id:'conversation-new', project_id:'project-1', title:'Shared'};
    };
  `, context);
  await context.createConversation();
  assert.strictEqual(context.__conversationCapture.path, '/api/v1/projects/project-1/conversations');
  assert.strictEqual(JSON.parse(context.__conversationCapture.options.body).agent_id, 'agent-1');

  vm.runInContext(`
    state.role = 'editor'; state.user = {id:'user-a', display_name:'User A'}; state.project = {id:'project-1'};
    state.activeConversation = {id:'conversation-1'}; state.lastRunId = 'run-retry'; state.lastRunOwnerId = 'user-a'; state.retrying = false;
    clearError = () => {}; setError = () => {}; renderMessages = () => {}; beginRun = () => {}; setRunState = () => {}; setSending = () => {};
    __retryCalls = 0; __retryGate = new Promise((resolve) => { __releaseRetry = resolve; });
    api = async () => { __retryCalls += 1; await __retryGate; throw new Error('stop'); };
    __retryFirst = retryRun(); __retrySecond = retryRun(); __retryDisabled = els.retryButton.disabled;
  `, context);
  assert.strictEqual(context.__retryCalls, 1, 'double retry must issue one request');
  assert.strictEqual(context.__retryDisabled, true, 'retry button must be disabled in flight');
  vm.runInContext('__releaseRetry()', context);
  await Promise.all([context.__retryFirst, context.__retrySecond]);
  assert.strictEqual(context.__retryCalls, 1, 'retry must remain deduplicated after completion');
  assert.strictEqual(vm.runInContext('state.retrying', context), false, 'retry flag must reset');

  vm.runInContext(`
    state.project = {id:'project-1', name:'Project One'}; state.conversations = [{id:'conversation-complete'}];
    state.activeConversation = {id:'conversation-complete'}; state.messages = [{sender_type:'agent', run_id:'run-complete'}];
    state.terminalRuns = new Set(); __beginArgs = null; setRunState = (kind, text) => { __runState = [kind, text]; };
    beginRun = (...args) => { __beginArgs = args; };
    localStorage.setItem('workspace.activeRun', JSON.stringify({runId:'run-complete', conversationId:'conversation-complete', projectId:'project-1', lastEventId:4, partial:'done'}));
  `, context);
  await context.restoreActiveRun();
  assert.strictEqual(localStorage.getItem('workspace.activeRun'), null, 'completed restore must clear saved run');
  assert.strictEqual(context.__beginArgs, null, 'completed restore must not reopen SSE');
  assert.deepStrictEqual(Array.from(context.__runState), ['success', '已完成']);

  vm.runInContext(`
    state.messages = [{sender_type:'user', run_id:'run-running', sender_id:'user-b'}]; __beginArgs = null;
    localStorage.setItem('workspace.activeRun', JSON.stringify({runId:'run-running', conversationId:'conversation-complete', projectId:'project-1', ownerId:'user-b', targetAgentId:'agent-1', lastEventId:9, partial:'part'}));
  `, context);
  await context.restoreActiveRun();
  assert.deepStrictEqual(Array.from(context.__beginArgs), ['run-running', 'conversation-complete', 9, 'part', 'user-b', 'agent-1']);

  vm.runInContext(`
    state.run = null; state.activeConversation = {id:'conversation-shared'};
    state.messages = [{sender_type:'user', sender_id:'user-b', run_id:'run-observed'}]; state.terminalRuns = new Set();
    beginRun = (...args) => { __observedArgs = args; };
    api = async (path) => { __observedRunPath = path; return {status:'running', agent_id:'agent-real', initiated_by_user_id:'user-b'}; };
  `, context);
  await context.discoverRunFromMessages();
  assert.strictEqual(context.__observedRunPath, '/api/v1/runs/run-observed', 'fallback discovery must read real Run metadata');
  assert.deepStrictEqual(Array.from(context.__observedArgs), ['run-observed', 'conversation-shared', 0, '', 'user-b', 'agent-real']);

  let eventSourceUrl = '';
  context.EventSource = class {
    constructor(url) { eventSourceUrl = url; this.readyState = 0; }
    addEventListener() {}
    close() {}
  };
  vm.runInContext(`
    state.user = {id:'user-a'}; state.role = 'editor'; state.eventSource = null;
    state.run = {runId:'run-running', conversationId:'conversation-complete', ownerId:'user-a', targetAgentId:'agent-1', lastEventId:9, partial:'part', reconnecting:false};
    updateStreamingNode = () => {}; connectRun();
  `, context);
  assert.ok(eventSourceUrl.includes('after=9'), 'SSE reconnect must resume after saved cursor');
  context.handleRunEvent('message.delta', {data: JSON.stringify({seq: 10, payload: {delta: 'ial'}})}, 'run-running');
  context.handleRunEvent('message.delta', {data: JSON.stringify({seq: 10, payload: {delta: 'ial'}})}, 'run-running');
  assert.strictEqual(vm.runInContext('state.run.lastEventId', context), 10, 'SSE cursor must advance');
  assert.strictEqual(vm.runInContext('state.run.partial', context), 'partial', 'duplicate SSE events must be deduplicated');

  let conversationSourceUrl = '';
  context.EventSource = class {
    constructor(url) { conversationSourceUrl = url; this.readyState = 0; }
    addEventListener() {}
    close() {}
  };
  vm.runInContext(`
    localStorage.removeItem('workspace.conversationEventCursor.conversation-events');
    state.activeConversation = {id:'conversation-events'}; state.conversationEventSource = null; state.conversationEventCursor = 0;
    state.messages = []; state.run = null; state.terminalRuns = new Set(); __conversationEvents = 0; __conversationArgs = null;
    renderMessages = () => {}; applyPermissions = () => {}; persistRun = () => {};
    beginRun = (...args) => { __conversationEvents += 1; __conversationArgs = args; };
    connectConversationStream();
  `, context);
  assert.ok(conversationSourceUrl.includes('/api/v1/conversations/conversation-events/events'), 'Conversation stream URL missing');
  assert.ok(!conversationSourceUrl.includes('after=0'), 'first connection should start at the server tail');
  const conversationEvent = JSON.stringify({schema_version:'1.0', conversation_id:'conversation-events', seq:7, type:'run.available', payload:{run_id:'run-event', target_agent_id:'agent-real', initiated_by_user_id:'user-b', status:'queued'}});
  context.handleConversationEvent({data:conversationEvent}, 'conversation-events');
  context.handleConversationEvent({data:conversationEvent}, 'conversation-events');
  assert.strictEqual(vm.runInContext('__conversationEvents', context), 1, 'duplicate Conversation events must be ignored');
  assert.deepStrictEqual(Array.from(context.__conversationArgs), ['run-event', 'conversation-events', 0, '', 'user-b', 'agent-real']);
  assert.strictEqual(localStorage.getItem('workspace.conversationEventCursor.conversation-events'), '7');

  let pollCallback = null;
  context.setInterval = (callback) => { pollCallback = callback; return 1; };
  vm.runInContext(`
    document.hidden = true; state.pollTimer = null; state.activeConversation = {id:'conversation-hidden'}; __pollLoads = 0; __originalLoadMessages = loadMessages;
    loadMessages = async () => { __pollLoads += 1; return false; }; startPolling();
  `, context);
  assert.ok(pollCallback, 'active conversation must schedule background discovery polling');
  await pollCallback();
  assert.strictEqual(vm.runInContext('__pollLoads', context), 1, 'hidden pages must continue shared message discovery');

  vm.runInContext(`
    state.activeConversation = {id:'conversation-race'}; state.run = null; state.messages = []; state.messageLoadSeq = 0; loadMessages = __originalLoadMessages;
    __loadResolvers = []; renderMessages = () => {}; api = () => new Promise((resolve) => __loadResolvers.push(resolve));
    __loadFirst = loadMessages(false); __loadSecond = loadMessages(false);
  `, context);
  context.__loadResolvers[1]({items:[{id:'newer'}]});
  await context.__loadSecond;
  context.__loadResolvers[0]({items:[{id:'older'}]});
  await context.__loadFirst;
  assert.deepStrictEqual(Array.from(vm.runInContext('state.messages.map((message) => message.id)', context)), ['newer'], 'older message response must not overwrite newer history');

  sidebarClasses.add('open');
  vm.runInContext(`
    state.run = null;
    state.eventSource = null;
    state.project = {id:'project-sidebar', name:'Project'};
    state.role = 'owner';
    state.projectAgents = [];
    state.conversations = [{id:'conversation-sidebar', agent_name:'Agent', title:'Sidebar'}];
    selectConversation = __originalSelectConversation;
    __sidebarFound = state.conversations.find((item) => item.id === 'conversation-sidebar')?.id;
    __sidebarSourceHasClose = selectConversation.toString().includes('classList.remove');
    renderConversations = () => {};
    loadMessages = async () => {};
    startPolling = () => {};
    connectRun = () => {};
    clearError = () => {};
    __sidebarClose = selectConversation('conversation-sidebar');
  `, context);
  assert.strictEqual(context.__sidebarFound, 'conversation-sidebar');
  assert.ok(context.__sidebarSourceHasClose);
  await context.__sidebarClose;
  assert.strictEqual(sidebarClasses.has('open'), false, 'selecting a conversation must close mobile sidebar');

  console.log(JSON.stringify({status:'PASS', role_permissions:true, target_agent_payload:true, project_conversation_route:true, retry_dedupe:true, refresh_restore:true, shared_run_discovery:true, conversation_sse:true, conversation_event_real_agent:true, conversation_event_dedupe:true, background_polling:true, out_of_order_history:true, sse_reconnect:true, sse_dedupe:true, mobile_sidebar_close:true}));
})().catch((error) => { console.error(error); process.exitCode = 1; });
