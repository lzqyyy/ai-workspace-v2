'use strict';

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {spawn} = require('child_process');

const BASE_URL = process.env.BASE_URL || 'http://localhost:8080';
const screenshotPath = path.resolve(process.env.FRONTEND_E2E_SCREENSHOT || 'docs/evidence/ui-project-first.png');
const browserPath = process.env.BROWSER_BIN || [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
].find(fs.existsSync);
if (!browserPath) throw new Error('No Edge/Chrome binary found; set BROWSER_BIN.');

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

class CDP {
  constructor(url) { this.url = url; this.id = 0; this.pending = new Map(); this.exceptions = []; }
  async connect() {
    this.socket = new WebSocket(this.url);
    await new Promise((resolve, reject) => {
      this.socket.addEventListener('open', resolve, {once: true});
      this.socket.addEventListener('error', reject, {once: true});
    });
    this.socket.addEventListener('message', (event) => {
      const message = JSON.parse(String(event.data));
      if (message.id && this.pending.has(message.id)) {
        const pending = this.pending.get(message.id);
        this.pending.delete(message.id);
        if (message.error) pending.reject(new Error(message.error.message)); else pending.resolve(message.result);
      } else if (message.method === 'Runtime.exceptionThrown') {
        this.exceptions.push(message.params.exceptionDetails?.text || 'runtime exception');
      }
    });
  }
  send(method, params = {}, sessionId) {
    const id = ++this.id;
    const payload = {id, method, params};
    if (sessionId) payload.sessionId = sessionId;
    this.socket.send(JSON.stringify(payload));
    return new Promise((resolve, reject) => this.pending.set(id, {resolve, reject}));
  }
  close() { this.socket?.close(); }
}

(async () => {
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'ai-workspace-browser-'));
  const browser = spawn(browserPath, [
    '--headless=new', '--remote-debugging-port=0', '--remote-allow-origins=*', `--user-data-dir=${profile}`,
    '--no-first-run', '--disable-gpu', '--window-size=1440,1100', 'about:blank'
  ], {stdio: 'ignore'});
  let cdp;
  try {
    const portFile = path.join(profile, 'DevToolsActivePort');
    for (let attempt = 0; attempt < 200 && !fs.existsSync(portFile); attempt += 1) await sleep(100);
    if (!fs.existsSync(portFile)) throw new Error('Browser DevTools port did not become ready');
    const [port, wsPath] = fs.readFileSync(portFile, 'utf8').trim().split(/\r?\n/);
    cdp = new CDP(`ws://127.0.0.1:${port}${wsPath}`);
    await cdp.connect();
    const target = await cdp.send('Target.createTarget', {url: 'about:blank'});
    const attached = await cdp.send('Target.attachToTarget', {targetId: target.targetId, flatten: true});
    const session = attached.sessionId;
    await cdp.send('Page.enable', {}, session);
    await cdp.send('Runtime.enable', {}, session);
    await cdp.send('Page.navigate', {url: `${BASE_URL}/`}, session);

    const evaluate = async (expression) => {
      const result = await cdp.send('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true}, session);
      if (result.exceptionDetails) throw new Error(result.exceptionDetails.text || 'page evaluation failed');
      return result.result.value;
    };
    const waitFor = async (expression, timeout = 20000, label = expression) => {
      const deadline = Date.now() + timeout;
      while (Date.now() < deadline) {
        try { if (await evaluate(expression)) return; } catch {}
        await sleep(150);
      }
      let debug = '';
      try {
        debug = await evaluate(`JSON.stringify({
          project: document.getElementById('project-name')?.textContent,
          projectError: document.getElementById('project-error')?.textContent,
          projectDialogOpen: document.getElementById('project-create-dialog')?.open,
          agentError: document.getElementById('agent-error')?.textContent,
          conversation: document.getElementById('conversation-title')?.textContent,
          runState: document.getElementById('run-state-text')?.textContent,
          body: document.body.innerText.slice(0, 1200)
        })`);
      } catch {}
      throw new Error(`Timed out waiting for ${label}: ${debug}`);
    };
    const unique = Date.now().toString(36);

    await waitFor("document.readyState === 'complete'");
    await evaluate(`(() => {
      const auth = document.getElementById('auth-view');
      if (auth && !auth.classList.contains('hidden')) {
        document.getElementById('username').value = 'demo';
        document.getElementById('password').value = 'demo1234';
        document.getElementById('login-form').requestSubmit();
      }
      return true;
    })()`);
    await waitFor("!document.getElementById('workspace-view').classList.contains('hidden')", 20000, 'workspace login');
    await waitFor("document.getElementById('project-name').textContent !== '未选择 Project'", 25000, 'Project load');

    await evaluate("document.getElementById('new-project').click()");
    await waitFor("document.getElementById('project-create-dialog').open");
    await evaluate(`(() => {
      document.getElementById('project-name-input').value = 'Browser E2E ${unique}';
      document.getElementById('project-description-input').value = 'Automated Project-first browser acceptance';
      document.getElementById('project-form').requestSubmit();
      return true;
    })()`);
    await waitFor(`document.getElementById('project-name').textContent === 'Browser E2E ${unique}'`, 20000, 'Project creation');

    await evaluate("document.getElementById('new-agent').click()");
    await waitFor("document.getElementById('agent-dialog').open");
    await evaluate(`(() => {
      document.getElementById('agent-name').value = 'Browser Agent ${unique}';
      document.getElementById('agent-prompt').value = 'Reply with the current user prompt.';
      document.getElementById('agent-form').requestSubmit();
      return true;
    })()`);
    await waitFor("document.querySelectorAll('#agent-select option').length > 0 && !document.getElementById('new-conversation').disabled", 20000, 'Agent add');

    await evaluate("document.getElementById('new-conversation').click()");
    await waitFor("document.getElementById('conversation-title').textContent.startsWith('共享对话') && !document.getElementById('message-input').disabled", 20000, 'Conversation creation');

    const prompt = `browser-e2e-${unique}`;
    await evaluate(`(() => {
      const input = document.getElementById('message-input');
      input.value = '${prompt}';
      document.getElementById('message-form').requestSubmit();
      return true;
    })()`);
    await waitFor("document.getElementById('run-state-text').textContent === '已完成'", 30000, 'Run completion');
    await waitFor(`Array.from(document.querySelectorAll('.message.agent .message-content')).some((node) => node.textContent.includes('${prompt}'))`, 30000, 'Agent reply');

    const result = await evaluate(`(() => ({
      title: document.getElementById('conversation-title').textContent,
      project: document.getElementById('project-name').textContent,
      role: document.getElementById('role-badge').textContent,
      messageCount: document.querySelectorAll('.message').length,
      agentText: Array.from(document.querySelectorAll('.message.agent .message-content')).map((node) => node.textContent),
      runState: document.getElementById('run-state-text').textContent,
      sendEnabled: !document.getElementById('send-button').disabled
    }))()`);
    assert.strictEqual(result.role, 'owner');
    assert.ok(result.messageCount >= 2, JSON.stringify(result));
    assert.strictEqual(result.runState, '已完成');
    assert.strictEqual(result.sendEnabled, true);
    if (cdp.exceptions.length) throw new Error(`Browser runtime exceptions: ${cdp.exceptions.join('; ')}`);

    fs.mkdirSync(path.dirname(screenshotPath), {recursive: true});
    const screenshot = await cdp.send('Page.captureScreenshot', {format: 'png', captureBeyondViewport: false}, session);
    fs.writeFileSync(screenshotPath, Buffer.from(screenshot.data, 'base64'));
    console.log(JSON.stringify({status: 'PASS', project: result.project, conversation: result.title, role: result.role, message_count: result.messageCount, run_state: result.runState, screenshot: screenshotPath}));
  } finally {
    try { cdp?.close(); } catch {}
    browser.kill();
    await sleep(250);
    fs.rmSync(profile, {recursive: true, force: true});
  }
})().catch((error) => { console.error(error.stack || error); process.exitCode = 1; });
