'use strict';

const assert = require('assert');
const fs = require('fs');
const vm = require('vm');

function element() {
  const element = {
    disabled: false,
    hidden: false,
    className: '',
    textContent: '',
    value: '',
    style: {},
    classList: { add() {}, remove() {}, toggle() {} },
    addEventListener() {},
    append() {},
    replaceChildren() {},
    querySelector() { return null; },
    focus() {},
    showModal() {},
    close() {},
  };
  return element;
}

const elements = new Map();
const document = {
  getElementById(id) {
    if (!elements.has(id)) elements.set(id, element());
    return elements.get(id);
  },
  querySelectorAll() { return []; },
};
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
  crypto: { randomUUID: () => `00000000-0000-4000-8000-${String(++uuidCounter).padStart(12, '0')}` },
  window: { addEventListener() {} },
  setTimeout,
  clearTimeout,
  requestAnimationFrame: (callback) => callback(),
  Intl,
  Promise,
  Date,
  Number,
  Boolean,
  JSON,
  encodeURIComponent,
  EventSource: class { constructor() {} addEventListener() {} close() {} },
};
vm.createContext(context);
vm.runInContext(fs.readFileSync('frontend/app.js', 'utf8'), context);

(async () => {
  let releaseRetry;
  const retryGate = new Promise((resolve) => { releaseRetry = resolve; });
  context.__retryGate = retryGate;
  context.__retryCalls = 0;
  vm.runInContext(`
    setRunState = () => {};
    setSending = () => {};
    clearError = () => {};
    setError = () => {};
    renderMessages = () => {};
    beginRun = () => {};
    api = async function () { __retryCalls += 1; await __retryGate; throw new Error('stop'); };
    state.lastRunId = 'run-retry';
    state.activeConversation = { id: 'conversation-1' };
    state.run = null;
    __retryFirst = retryRun();
    __retrySecond = retryRun();
    __retryDisabledInFlight = els.retryButton.disabled;
  `, context);
  assert.strictEqual(context.__retryCalls, 1, 'double retry must issue one request');
  assert.strictEqual(context.__retryDisabledInFlight, true, 'retry button must be disabled in flight');
  releaseRetry();
  await Promise.all([context.__retryFirst, context.__retrySecond]);
  assert.strictEqual(context.__retryCalls, 1, 'retry must remain deduplicated after completion');
  assert.strictEqual(vm.runInContext('state.retrying', context), false, 'retry flag must reset');
  assert.strictEqual(elements.get('retry-button').disabled, false, 'retry button must recover');

  vm.runInContext(`
    state.run = null;
    state.lastRunId = null;
    state.conversations = [{ id: 'conversation-complete' }];
    state.activeConversation = { id: 'conversation-complete' };
    state.messages = [{ sender_type: 'agent', run_id: 'run-complete' }];
    __runStates = [];
    __beginArgs = null;
    setRunState = (kind, text) => __runStates.push([kind, text]);
    setSending = () => {};
    beginRun = (...args) => { __beginArgs = args; };
    localStorage.setItem('workspace.activeRun', JSON.stringify({ runId: 'run-complete', conversationId: 'conversation-complete', lastEventId: 4, partial: 'done' }));
  `, context);
  await context.restoreActiveRun();
  assert.strictEqual(localStorage.getItem('workspace.activeRun'), null, 'completed restore must clear saved run');
  assert.strictEqual(context.__beginArgs, null, 'completed restore must not reopen SSE');
  assert.strictEqual(JSON.stringify(context.__runStates), JSON.stringify([['success', '已完成']]), 'completed restore must show completed state');

  vm.runInContext(`
    state.messages = [{ sender_type: 'user', run_id: 'run-running' }];
    __beginArgs = null;
    localStorage.setItem('workspace.activeRun', JSON.stringify({ runId: 'run-running', conversationId: 'conversation-complete', lastEventId: 9, partial: 'part' }));
  `, context);
  await context.restoreActiveRun();
  assert.strictEqual(JSON.stringify(context.__beginArgs), JSON.stringify(['run-running', 'conversation-complete', 9, 'part']), 'running restore must resume from saved cursor');

  let eventSourceUrl = '';
  context.EventSource = class {
    constructor(url) { eventSourceUrl = url; this.readyState = 0; }
    addEventListener() {}
    close() {}
  };
  vm.runInContext(`
    updateStreamingNode = () => {};
    state.eventSource = null;
    state.run = { runId: 'run-running', conversationId: 'conversation-complete', lastEventId: 9, partial: 'part', reconnecting: false };
    connectRun();
  `, context);
  assert.ok(eventSourceUrl.includes('after=9'), 'SSE reconnect must resume after the saved cursor');

  context.handleRunEvent('message.delta', {
    data: JSON.stringify({ seq: 10, payload: { delta: 'ial' } }),
  }, 'run-running');
  assert.strictEqual(vm.runInContext('state.run.lastEventId', context), 10, 'SSE cursor must advance');
  assert.strictEqual(vm.runInContext('state.run.partial', context), 'partial', 'partial content must survive reconnect');
  const persisted = JSON.parse(localStorage.getItem('workspace.activeRun'));
  assert.strictEqual(persisted.lastEventId, 10, 'persisted cursor must match SSE progress');
  assert.strictEqual(persisted.partial, 'partial', 'persisted partial content must match stream progress');

  console.log(JSON.stringify({ status: 'PASS', retry_dedupe: true, refresh_restore: true, sse_reconnect: true, completed_state: true }));
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
