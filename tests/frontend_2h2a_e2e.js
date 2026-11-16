'use strict';

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {spawn} = require('child_process');

const BASE_URL = process.env.BASE_URL || 'http://localhost:8080';
const screenshotPath = path.resolve(process.env.FRONTEND_2H2A_SCREENSHOT || 'docs/evidence/ui-2h2a.png');
const browserPath = process.env.BROWSER_BIN || [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
].find(fs.existsSync);
if (!browserPath) throw new Error('No Edge/Chrome binary found; set BROWSER_BIN.');
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

class CDP {
  constructor(url) { this.url=url; this.id=0; this.pending=new Map(); this.events=[]; this.exceptions=[]; }
  async connect() {
    this.socket=new WebSocket(this.url);
    await new Promise((resolve,reject)=>{ this.socket.addEventListener('open',resolve,{once:true}); this.socket.addEventListener('error',reject,{once:true}); });
    this.socket.addEventListener('message',(event)=>{
      const message=JSON.parse(String(event.data));
      if(message.id&&this.pending.has(message.id)){ const pending=this.pending.get(message.id); this.pending.delete(message.id); if(message.error) pending.reject(new Error(message.error.message)); else pending.resolve(message.result); return; }
      this.events.push(message);
      if(message.method==='Runtime.exceptionThrown') this.exceptions.push(message.params.exceptionDetails?.text||'runtime exception');
    });
  }
  send(method,params={},sessionId){ const id=++this.id; const payload={id,method,params}; if(sessionId) payload.sessionId=sessionId; this.socket.send(JSON.stringify(payload)); return new Promise((resolve,reject)=>this.pending.set(id,{resolve,reject})); }
  close(){ this.socket?.close(); }
}

(async()=>{
  const profile=fs.mkdtempSync(path.join(os.tmpdir(),'ai-workspace-2h2a-'));
  const browser=spawn(browserPath,['--headless=new','--remote-debugging-port=0','--remote-allow-origins=*',`--user-data-dir=${profile}`,'--no-first-run','--disable-gpu','--window-size=1360,980','about:blank'],{stdio:'ignore'});
  let cdp;
  try{
    const portFile=path.join(profile,'DevToolsActivePort');
    for(let i=0;i<200&&!fs.existsSync(portFile);i+=1) await sleep(100);
    if(!fs.existsSync(portFile)) throw new Error('Browser DevTools port did not become ready');
    const [port,wsPath]=fs.readFileSync(portFile,'utf8').trim().split(/\r?\n/);
    cdp=new CDP(`ws://127.0.0.1:${port}${wsPath}`); await cdp.connect();

    async function openPage(label){
      const context=await cdp.send('Target.createBrowserContext');
      const target=await cdp.send('Target.createTarget',{url:'about:blank',browserContextId:context.browserContextId});
      const attached=await cdp.send('Target.attachToTarget',{targetId:target.targetId,flatten:true});
      const session=attached.sessionId;
      await cdp.send('Page.enable',{},session); await cdp.send('Runtime.enable',{},session); await cdp.send('Network.enable',{},session);
      await cdp.send('Emulation.setDeviceMetricsOverride',{width:1360,height:980,deviceScaleFactor:1,mobile:false},session);
      const evaluate=async(expression)=>{ const result=await cdp.send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true},session); if(result.exceptionDetails) throw new Error(result.exceptionDetails.text||`${label}: page evaluation failed`); return result.result.value; };
      const waitFor=async(expression,timeout=25000,labelText=expression)=>{ const deadline=Date.now()+timeout; while(Date.now()<deadline){ try{ if(await evaluate(expression)) return; }catch{} await sleep(150); } let debug=''; try{ debug=await evaluate(`JSON.stringify({project:document.getElementById('project-name')?.textContent,conversation:document.getElementById('conversation-title')?.textContent,runState:document.getElementById('run-state-text')?.textContent,body:document.body.innerText.slice(0,1000)})`); }catch{} throw new Error(`${label}: timed out waiting for ${labelText}: ${debug}`); };
      await cdp.send('Page.navigate',{url:`${BASE_URL}/`},session);
      await waitFor("document.readyState === 'complete'",20000,'page load');
      return {label,session,evaluate,waitFor};
    }
    async function login(page,username){
      await page.evaluate(`(()=>{ document.getElementById('username').value='${username}'; document.getElementById('password').value='demo1234'; document.getElementById('login-form').requestSubmit(); return true; })()`);
      await page.waitFor("!document.getElementById('workspace-view').classList.contains('hidden')",20000,`${page.label} login`);
    }
    async function api(page,method,url,body){
      const literal=JSON.stringify({method,url,body:body??null});
      return page.evaluate(`(async()=>{ const input=${literal}; const headers={'X-Request-ID':'browser-'+crypto.randomUUID()}; if(input.body!==null) headers['Content-Type']='application/json'; const response=await fetch(input.url,{method:input.method,headers,credentials:'same-origin',body:input.body===null?undefined:JSON.stringify(input.body)}); const text=await response.text(); if(!response.ok) throw new Error(response.status+':'+text); return text?JSON.parse(text):null; })()`);
    }
    async function selectProject(page,projectId){
      await page.evaluate(`localStorage.setItem('workspace.projectId','${projectId}'); location.reload(); true`);
      await page.waitFor("document.readyState === 'complete'",20000,`${page.label} reload`);
      await page.waitFor("!document.getElementById('workspace-view').classList.contains('hidden')",20000,`${page.label} workspace reload`);
      await page.waitFor("document.getElementById('project-name').textContent !== '未选择 Project'",25000,`${page.label} project load`);
    }
    async function setAgent(page,agentId){ await page.evaluate(`(()=>{ const select=document.getElementById('agent-select'); select.value='${agentId}'; select.dispatchEvent(new Event('change',{bubbles:true})); return select.value; })()`); }
    async function sendInUi(page,prompt){ await page.evaluate(`(()=>{ const input=document.getElementById('message-input'); input.value=${JSON.stringify(prompt)}; document.getElementById('message-form').requestSubmit(); return true; })()`); }

    const ownerPage=await openPage('owner');
    const editorPage=await openPage('editor');
    await login(ownerPage,'demo'); await login(editorPage,'other');
    const editorUser=await api(editorPage,'GET','/api/v1/me');
    const unique=Date.now().toString(36);
    const project=await api(ownerPage,'POST','/api/v1/projects',{name:`2H2A UI ${unique}`,description:'Two humans, two Agents browser acceptance'});
    await api(ownerPage,'POST',`/api/v1/projects/${project.id}/members`,{user_id:editorUser.id,role:'editor'});
    const ownerAgent=await api(ownerPage,'POST','/api/v1/agents',{name:`Owner UI Agent ${unique}`,system_prompt:'Reply as Owner UI Agent.'});
    await api(ownerPage,'POST',`/api/v1/projects/${project.id}/agents`,{agent_id:ownerAgent.id});
    const editorAgent=await api(editorPage,'POST','/api/v1/agents',{name:`Editor UI Agent ${unique}`,system_prompt:'Reply as Editor UI Agent.'});
    await api(editorPage,'POST',`/api/v1/projects/${project.id}/agents`,{agent_id:editorAgent.id});
    const conversation=await api(ownerPage,'POST',`/api/v1/projects/${project.id}/conversations`,{agent_id:ownerAgent.id,title:`Shared UI Conversation ${unique}`});

    await selectProject(ownerPage,project.id); await selectProject(editorPage,project.id);
    await ownerPage.waitFor(`document.getElementById('conversation-title').textContent === '${conversation.title}'`,25000,'owner shared conversation');
    await editorPage.waitFor(`document.getElementById('conversation-title').textContent === '${conversation.title}'`,25000,'editor shared conversation');

    await setAgent(ownerPage,ownerAgent.id);
    const ownerPrompt=`owner-to-agent-${unique}`;
    await sendInUi(ownerPage,ownerPrompt);
    await ownerPage.waitFor("document.getElementById('run-state-text').textContent === '已完成'",30000,'owner run complete');
    await editorPage.waitFor(`Array.from(document.querySelectorAll('.message.user .message-content')).some((node)=>node.textContent.includes('${ownerPrompt}'))`,30000,'editor sees owner message');
    await editorPage.waitFor(`Array.from(document.querySelectorAll('.message.agent .message-content')).some((node)=>node.textContent.includes('${ownerPrompt}'))`,30000,'editor sees owner Agent reply');
    await editorPage.waitFor(`Array.from(document.querySelectorAll('.message.agent .message-meta strong')).some((node)=>node.textContent.includes('${ownerAgent.name}'))`,30000,'owner Agent identity');
    const editorSseUrls=cdp.events.filter((event)=>event.sessionId===editorPage.session&&event.method==='Network.responseReceived'&&String(event.params?.response?.url||'').includes(`/api/v1/runs/`)&&String(event.params?.response?.url||'').includes('/events')).map((event)=>event.params.response.url);
    assert.ok(editorSseUrls.length>0,'editor must subscribe to the shared Run SSE');

    await setAgent(editorPage,editorAgent.id);
    const editorPrompt=`editor-to-agent-${unique}`;
    await sendInUi(editorPage,editorPrompt);
    await editorPage.waitFor("document.getElementById('run-state-text').textContent === '已完成'",30000,'editor run complete');
    await ownerPage.waitFor(`Array.from(document.querySelectorAll('.message.user .message-content')).some((node)=>node.textContent.includes('${editorPrompt}'))`,30000,'owner sees editor message');
    await ownerPage.waitFor(`Array.from(document.querySelectorAll('.message.agent .message-content')).some((node)=>node.textContent.includes('${editorPrompt}'))`,30000,'owner sees editor Agent reply');
    await ownerPage.waitFor(`Array.from(document.querySelectorAll('.message.agent .message-meta strong')).some((node)=>node.textContent.includes('${editorAgent.name}'))`,30000,'editor Agent identity');
    const ownerSseUrls=cdp.events.filter((event)=>event.sessionId===ownerPage.session&&event.method==='Network.responseReceived'&&String(event.params?.response?.url||'').includes(`/api/v1/runs/`)&&String(event.params?.response?.url||'').includes('/events')).map((event)=>event.params.response.url);
    assert.ok(ownerSseUrls.length>0,'owner must subscribe to the shared Run SSE');

    const ownerResult=await ownerPage.evaluate(`(()=>({messages:Array.from(document.querySelectorAll('.message .message-content')).map((node)=>node.textContent),agents:Array.from(document.querySelectorAll('.message.agent .message-meta strong')).map((node)=>node.textContent),role:document.getElementById('role-badge').textContent}))()`);
    const editorResult=await editorPage.evaluate(`(()=>({messages:Array.from(document.querySelectorAll('.message .message-content')).map((node)=>node.textContent),agents:Array.from(document.querySelectorAll('.message.agent .message-meta strong')).map((node)=>node.textContent),role:document.getElementById('role-badge').textContent}))()`);
    assert.strictEqual(ownerResult.role,'owner'); assert.strictEqual(editorResult.role,'editor');
    assert.ok(ownerResult.messages.includes(ownerPrompt)&&ownerResult.messages.includes(editorPrompt),JSON.stringify(ownerResult));
    assert.ok(editorResult.messages.includes(ownerPrompt)&&editorResult.messages.includes(editorPrompt),JSON.stringify(editorResult));
    assert.ok(ownerResult.agents.includes(ownerAgent.name)&&ownerResult.agents.includes(editorAgent.name),JSON.stringify(ownerResult));
    assert.ok(editorResult.agents.includes(ownerAgent.name)&&editorResult.agents.includes(editorAgent.name),JSON.stringify(editorResult));

    await api(ownerPage,'PATCH',`/api/v1/projects/${project.id}/members/${editorUser.id}`,{role:'viewer'});
    await editorPage.evaluate('location.reload(); true');
    await editorPage.waitFor("document.readyState === 'complete'",20000,'viewer reload');
    await editorPage.waitFor("document.getElementById('role-badge').textContent === 'viewer'",25000,'viewer role');
    await editorPage.waitFor("document.getElementById('message-input').disabled && document.getElementById('new-conversation').disabled && !document.getElementById('readonly-note').classList.contains('hidden')",20000,'viewer readonly UI');
    const viewerControls=await editorPage.evaluate(`(()=>({inputDisabled:document.getElementById('message-input').disabled,sendDisabled:document.getElementById('send-button').disabled,conversationDisabled:document.getElementById('new-conversation').disabled,retryHidden:document.getElementById('retry-button').classList.contains('hidden')}))()`);
    assert.strictEqual(viewerControls.inputDisabled,true); assert.strictEqual(viewerControls.sendDisabled,true); assert.strictEqual(viewerControls.conversationDisabled,true); assert.strictEqual(viewerControls.retryHidden,true);
    if(cdp.exceptions.length) throw new Error(`Browser runtime exceptions: ${cdp.exceptions.join('; ')}`);

    fs.mkdirSync(path.dirname(screenshotPath),{recursive:true});
    const screenshot=await cdp.send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false},editorPage.session);
    fs.writeFileSync(screenshotPath,Buffer.from(screenshot.data,'base64'));
    console.log(JSON.stringify({status:'PASS',project_id:project.id,conversation_id:conversation.id,owner_agent:ownerAgent.name,editor_agent:editorAgent.name,owner_sees_shared_history:true,editor_sees_shared_history:true,owner_sse:ownerSseUrls.length>0,editor_sse:editorSseUrls.length>0,viewer_ui_readonly:true,screenshot:screenshotPath}));
  } finally {
    try{cdp?.close();}catch{} browser.kill(); await sleep(250); fs.rmSync(profile,{recursive:true,force:true});
  }
})().catch((error)=>{console.error(error.stack||error);process.exitCode=1;});
