const $ = (id) => document.getElementById(id);
const els = {
  auth: $('auth-view'), workspace: $('workspace-view'), loginForm: $('login-form'), loginButton: $('login-button'), loginError: $('login-error'),
  logout: $('logout-button'), userName: $('user-name'), userAvatar: $('user-avatar'), agentSelect: $('agent-select'), newAgent: $('new-agent'),
  newConversation: $('new-conversation'), conversationList: $('conversation-list'), conversationCount: $('conversation-count'),
  conversationTitle: $('conversation-title'), conversationSubtitle: $('conversation-subtitle'), runState: $('run-state'), runStateText: $('run-state-text'),
  messages: $('messages'), emptyState: $('empty-state'), emptyAction: $('empty-action'), messageForm: $('message-form'), messageInput: $('message-input'),
  sendButton: $('send-button'), cancelButton: $('cancel-button'), inlineError: $('inline-error'), inlineErrorText: $('inline-error-text'),
  retryButton: $('retry-button'), agentDialog: $('agent-dialog'), agentForm: $('agent-form'), agentName: $('agent-name'),
  agentPrompt: $('agent-prompt'), agentError: $('agent-error'), agentSubmit: $('agent-submit'), toast: $('toast')
};
const state = { user:null, agents:[], conversations:[], activeConversation:null, messages:[], agentId:null, run:null, eventSource:null, lastRunId:null, toastTimer:null };
class ApiError extends Error { constructor(code,message,status,retryable=false){super(message);this.code=code;this.status=status;this.retryable=retryable;} }
function uuid(){ if(crypto.randomUUID) return crypto.randomUUID(); return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g,c=>{const r=Math.random()*16|0;return(c==='x'?r:(r&3|8)).toString(16);}); }
async function api(path,options={}){
  const requestId=options.requestId||`web-${uuid()}`; const headers=new Headers(options.headers||{}); headers.set('X-Request-ID',requestId);
  if(options.body&&!headers.has('Content-Type')) headers.set('Content-Type','application/json');
  let response; try{response=await fetch(path,{...options,headers,credentials:'same-origin'});}catch{throw new ApiError('NETWORK_ERROR','无法连接服务，请检查网络后重试',0,true);}
  if(response.status===204) return null; let data=null; try{data=await response.json();}catch{}
  if(!response.ok) throw new ApiError(data?.code||'INTERNAL',data?.message||'请求失败，请稍后重试',response.status,Boolean(data?.retryable));
  return data;
}
function showAuth(){closeRunStream();els.workspace.classList.add('hidden');els.auth.classList.remove('hidden');els.loginButton.disabled=false;setTimeout(()=>$('username').focus(),20);}
function showWorkspace(){els.auth.classList.add('hidden');els.workspace.classList.remove('hidden');els.messageInput.focus();}
function toast(message){clearTimeout(state.toastTimer);els.toast.textContent=message;els.toast.classList.remove('hidden');state.toastTimer=setTimeout(()=>els.toast.classList.add('hidden'),2600);}
function formatTime(value){if(!value)return'';const date=new Date(value);if(Number.isNaN(date.getTime()))return'';return new Intl.DateTimeFormat('zh-CN',{hour:'2-digit',minute:'2-digit'}).format(date);}
function setRunState(kind,text){els.runState.className=`run-state ${kind||''}`.trim();els.runStateText.textContent=text;}
function setError(message,retryable=false){els.inlineErrorText.textContent=message||'';els.inlineError.classList.toggle('hidden',!message);els.retryButton.classList.toggle('hidden',!(retryable&&state.lastRunId));}
function clearError(){setError('');}
function activeRun(){return state.run&&state.run.conversationId===state.activeConversation?.id?state.run:null;}
function setSending(busy){const locked=busy||Boolean(state.run);els.sendButton.disabled=locked||!state.activeConversation;els.messageInput.disabled=locked||!state.activeConversation;els.cancelButton.classList.toggle('hidden',!(busy&&activeRun()));}
async function login(event){
  event.preventDefault();els.loginError.textContent='';els.loginButton.disabled=true;
  try{await api('/api/v1/auth/login',{method:'POST',body:JSON.stringify({username:$('username').value.trim(),password:$('password').value})});await bootstrap();}
  catch(error){els.loginError.textContent=error.code==='AUTH_INVALID'?'用户名或密码错误':error.message;}
  finally{els.loginButton.disabled=false;}
}
async function logout(){try{await api('/api/v1/auth/logout',{method:'POST'});}catch{}localStorage.removeItem('workspace.activeRun');state.user=null;state.agents=[];state.conversations=[];state.messages=[];state.activeConversation=null;state.run=null;showAuth();}
async function bootstrap(){try{state.user=await api('/api/v1/me');els.userName.textContent=state.user.display_name;els.userAvatar.textContent=(state.user.display_name||'U').slice(0,1).toUpperCase();showWorkspace();await loadWorkspace();await restoreActiveRun();}catch(error){if(error.status===401){showAuth();return;}throw error;}}
async function loadWorkspace(){
  const [agentsResponse,conversationsResponse]=await Promise.all([api('/api/v1/agents'),api('/api/v1/conversations')]);
  state.agents=agentsResponse.items||[];state.conversations=conversationsResponse.items||[];renderAgents();renderConversations();
  if(!state.agents.length){updateEmptyState();showAgentDialog();return;}
  const remembered=localStorage.getItem('workspace.agentId');state.agentId=state.agents.some(a=>a.id===remembered)?remembered:state.agents[0].id;els.agentSelect.value=state.agentId;
  if(state.conversations.length)await selectConversation(state.conversations[0].id);else updateEmptyState();
}
function renderAgents(){els.agentSelect.replaceChildren();for(const agent of state.agents){const option=document.createElement('option');option.value=agent.id;option.textContent=agent.name;els.agentSelect.append(option);}els.agentSelect.disabled=state.agents.length===0;}
function renderConversations(){
  els.conversationList.replaceChildren();els.conversationCount.textContent=String(state.conversations.length);
  for(const conversation of state.conversations){const button=document.createElement('button');button.type='button';button.className='conversation-item';if(state.activeConversation?.id===conversation.id)button.classList.add('active');const title=document.createElement('strong');title.textContent=conversation.title;const meta=document.createElement('span');meta.textContent=`${conversation.agent_name} · ${formatTime(conversation.updated_at)}`;button.append(title,meta);button.addEventListener('click',()=>selectConversation(conversation.id));els.conversationList.append(button);}
}
function updateEmptyState(){
  const hasAgent=state.agents.length>0;els.emptyState.classList.remove('hidden');els.messages.classList.add('hidden');els.emptyState.querySelector('h3').textContent=hasAgent?'创建第一个会话':'先创建一个 Agent';els.emptyState.querySelector('p').textContent=hasAgent?'Agent 已就绪。创建会话后即可发送消息，并由 Worker 流式回复。':'Agent 绑定所有者与模型配置，是发起会话的前提。';els.emptyAction.textContent=hasAgent?'创建会话':'创建 Agent';els.messageInput.disabled=true;els.sendButton.disabled=true;
}async function selectConversation(id){
  const conversation=state.conversations.find(item=>item.id===id);if(!conversation)return;
  if(state.activeConversation?.id!==id||state.eventSource)closeRunStream();
  state.activeConversation=conversation;state.lastRunId=state.run?.conversationId===id?state.run.runId:null;renderConversations();els.emptyState.classList.add('hidden');els.messages.classList.remove('hidden');
  els.conversationTitle.textContent=conversation.title;els.conversationSubtitle.textContent=`${conversation.agent_name} · 历史消息持久化于 PostgreSQL`;clearError();
  try{const response=await api(`/api/v1/conversations/${encodeURIComponent(id)}/messages`);if(state.activeConversation?.id!==id)return;state.messages=response.items||[];renderMessages();}
  catch(error){if(state.activeConversation?.id===id)setError(error.message,error.retryable);}
  if(activeRun()){setRunState('active','正在恢复事件流');setSending(true);connectRun();}
  else if(state.run){setRunState('active','另一会话运行中');setSending(false);}
  else{setRunState('','空闲');setSending(false);}
}
function buildMessage(message,streaming=false){
  const article=document.createElement('article');article.className=`message ${message.sender_type||'agent'}${streaming?' streaming':''}`;
  if(message.id)article.dataset.messageId=message.id;
  const avatar=document.createElement('div');avatar.className='message-avatar';avatar.textContent=message.sender_type==='user'?'YOU':'AI';
  const body=document.createElement('div');body.className='message-body';const meta=document.createElement('div');meta.className='message-meta';
  const sender=document.createElement('strong');sender.textContent=message.sender_type==='user'?(state.user?.display_name||'你'):(state.activeConversation?.agent_name||'Agent');const time=document.createElement('span');time.textContent=formatTime(message.created_at)||'刚刚';meta.append(sender,time);
  const content=document.createElement('div');content.className='message-content';content.textContent=message.content||'';
  body.append(meta,content);article.append(avatar,body);return article;
}
function renderMessages(){
  els.messages.replaceChildren();for(const message of state.messages)els.messages.append(buildMessage(message));
  if(state.run?.conversationId===state.activeConversation?.id){const pending=buildMessage({sender_type:'agent',content:state.run.partial||''},true);pending.id='streaming-message';els.messages.append(pending);}
  els.emptyState.classList.toggle('hidden',state.messages.length>0||Boolean(state.run));scrollToBottom();
}
function scrollToBottom(){requestAnimationFrame(()=>{const stage=$('message-stage');stage.scrollTop=stage.scrollHeight;});}
function persistRun(){
  if(!state.run){localStorage.removeItem('workspace.activeRun');return;}
  localStorage.setItem('workspace.activeRun',JSON.stringify({runId:state.run.runId,conversationId:state.run.conversationId,lastEventId:state.run.lastEventId,partial:state.run.partial}));
}
function closeRunStream(){if(state.eventSource){state.eventSource.close();state.eventSource=null;}setSending(false);}
async function sendMessage(event){
  event?.preventDefault();const content=els.messageInput.value.trim();if(!content||!state.activeConversation||state.run)return;
  clearError();setSending(true);setRunState('active','正在创建 Run');const requestId=`web-${uuid()}`;
  try{
    const response=await api(`/api/v1/conversations/${encodeURIComponent(state.activeConversation.id)}/messages`,{method:'POST',requestId,headers:{'Idempotency-Key':requestId},body:JSON.stringify({content,client_request_id:requestId})});
    els.messageInput.value='';els.messageInput.style.height='auto';state.messages.push(response.message);renderMessages();
    state.lastRunId=response.run_id;beginRun(response.run_id,state.activeConversation.id,0,'');
  }catch(error){setSending(false);setRunState('error','创建失败');setError(error.message,error.retryable);}
}
async function createConversation(){
  if(!state.agents.length){showAgentDialog();return;}
  try{
    const conversation=await api('/api/v1/conversations',{method:'POST',body:JSON.stringify({agent_id:state.agentId,title:`对话 ${new Date().toLocaleDateString('zh-CN')}`})});
    state.conversations.unshift(conversation);renderConversations();await selectConversation(conversation.id);toast('会话已创建');
  }catch(error){toast(error.message);}
}
function showAgentDialog(){els.agentError.textContent='';els.agentName.value='';els.agentPrompt.value='';els.agentDialog.showModal();setTimeout(()=>els.agentName.focus(),30);}
async function createAgent(event){
  event.preventDefault();els.agentError.textContent='';els.agentSubmit.disabled=true;
  try{
    const agent=await api('/api/v1/agents',{method:'POST',body:JSON.stringify({name:els.agentName.value.trim(),system_prompt:els.agentPrompt.value.trim()})});
    state.agents.unshift(agent);state.agentId=agent.id;localStorage.setItem('workspace.agentId',agent.id);renderAgents();els.agentSelect.value=agent.id;els.agentDialog.close();toast('Agent 已创建');if(!state.conversations.length)updateEmptyState();
  }catch(error){els.agentError.textContent=error.message;}
  finally{els.agentSubmit.disabled=false;}
}
async function cancelRun(){
  if(!state.run)return;els.cancelButton.disabled=true;setRunState('active','正在取消');
  try{await api(`/api/v1/runs/${encodeURIComponent(state.run.runId)}/cancel`,{method:'POST'});}
  catch(error){setError(error.message,false);}
  finally{els.cancelButton.disabled=false;}
}
async function retryRun(){
  if(!state.lastRunId)return;clearError();setRunState('active','正在重试');setSending(true);
  try{
    const response=await api(`/api/v1/runs/${encodeURIComponent(state.lastRunId)}/retry`,{method:'POST',headers:{'Idempotency-Key':`retry-${uuid()}`}});
    if(!state.messages.some(message=>message.id===response.message.id))state.messages.push(response.message);
    renderMessages();state.lastRunId=response.run_id;beginRun(response.run_id,state.activeConversation.id,0,'');
  }catch(error){setSending(false);setRunState('error','重试失败');setError(error.message,error.retryable);}
}function beginRun(runId,conversationId,lastEventId=0,partial=''){
  closeRunStream();state.run={runId,conversationId,lastEventId:Number(lastEventId)||0,partial:partial||'',reconnecting:false};persistRun();
  setSending(true);setRunState('active','Run 已排队');clearError();renderMessages();connectRun();
}
function connectRun(){
  if(!state.run)return;if(state.eventSource)state.eventSource.close();
  const run=state.run;const requestId=`web-sse-${uuid()}`;
  const source=new EventSource(`/api/v1/runs/${encodeURIComponent(run.runId)}/events?request_id=${encodeURIComponent(requestId)}&after=${run.lastEventId}`);state.eventSource=source;
  source.onopen=()=>{if(state.run?.runId===run.runId){state.run.reconnecting=false;setRunState('active','流式接收中');}};
  for(const name of ['run.started','message.delta','message.completed','run.failed','run.cancelled']){source.addEventListener(name,event=>handleRunEvent(name,event,run.runId));}
  source.onerror=()=>{
    if(!state.run||state.run.runId!==run.runId)return;
    if(source.readyState===EventSource.CONNECTING){state.run.reconnecting=true;setRunState('active','连接中断，正在回放');}
    else{setRunState('error','事件连接失败');setError('SSE 连接失败，请刷新页面或重试',false);}
  };
}
function handleRunEvent(name,event,runId){
  if(!state.run||state.run.runId!==runId)return;let envelope;try{envelope=JSON.parse(event.data);}catch{return;}
  state.run.lastEventId=Number(envelope.seq)||state.run.lastEventId;
  if(name==='run.started'){setRunState('active','Worker 执行中');}
  if(name==='message.delta'){state.run.partial+=envelope.payload?.delta||'';updateStreamingNode();}
  if(name==='message.completed'){state.run.partial=envelope.payload?.content||state.run.partial;updateStreamingNode();}
  persistRun();
  if(name==='message.completed')finishRun('succeeded',envelope);
  if(name==='run.failed')finishRun('failed',envelope);
  if(name==='run.cancelled')finishRun('cancelled',envelope);
}
function updateStreamingNode(){
  let node=$('streaming-message');if(!node){renderMessages();node=$('streaming-message');}
  if(node){const content=node.querySelector('.message-content');if(content)content.textContent=state.run?.partial||'';scrollToBottom();}
}
async function finishRun(kind,envelope){
  if(!state.run)return;const finishedRunId=state.run.runId;const conversationId=state.run.conversationId;
  if(state.eventSource){state.eventSource.close();state.eventSource=null;}
  state.run=null;state.lastRunId=finishedRunId;localStorage.removeItem('workspace.activeRun');setSending(false);
  if(state.activeConversation?.id===conversationId){
    if(kind==='succeeded'){setRunState('success','已完成');clearError();}
    if(kind==='failed'){const code=envelope.payload?.error_code||'MODEL_PROVIDER_ERROR';setRunState('error','运行失败');setError(`运行失败（${code}）。可以重新运行。`,true);}
    if(kind==='cancelled'){setRunState('error','已取消');setError('运行已取消。可以重新运行。',true);}
  }
  if(state.activeConversation?.id===conversationId){try{const response=await api(`/api/v1/conversations/${encodeURIComponent(conversationId)}/messages`);state.messages=response.items||[];renderMessages();}catch(error){setError(error.message,false);}}
  const conversation=state.conversations.find(item=>item.id===conversationId);if(conversation)conversation.updated_at=new Date().toISOString();renderConversations();
}
async function restoreActiveRun(){
  const raw=localStorage.getItem('workspace.activeRun');if(!raw)return;let saved;try{saved=JSON.parse(raw);}catch{localStorage.removeItem('workspace.activeRun');return;}
  const conversation=state.conversations.find(item=>item.id===saved.conversationId);if(!conversation){localStorage.removeItem('workspace.activeRun');return;}
  if(state.activeConversation?.id!==conversation.id)await selectConversation(conversation.id);
  const finalMessage=state.messages.find(message=>message.sender_type==='agent'&&message.run_id===saved.runId);
  if(finalMessage){localStorage.removeItem('workspace.activeRun');return;}
  state.lastRunId=saved.runId;beginRun(saved.runId,saved.conversationId,saved.lastEventId||0,saved.partial||'');
}
function autoResize(){els.messageInput.style.height='auto';els.messageInput.style.height=`${Math.min(els.messageInput.scrollHeight,180)}px`;}
function bindEvents(){
  els.loginForm.addEventListener('submit',login);els.logout.addEventListener('click',logout);els.messageForm.addEventListener('submit',sendMessage);
  els.newConversation.addEventListener('click',createConversation);els.newAgent.addEventListener('click',showAgentDialog);els.agentForm.addEventListener('submit',createAgent);
  els.cancelButton.addEventListener('click',cancelRun);els.retryButton.addEventListener('click',retryRun);
  els.emptyAction.addEventListener('click',()=>{if(state.agents.length)createConversation();else showAgentDialog();});
  els.agentSelect.addEventListener('change',()=>{state.agentId=els.agentSelect.value;localStorage.setItem('workspace.agentId',state.agentId);});
  els.messageInput.addEventListener('input',autoResize);
  els.messageInput.addEventListener('keydown',event=>{if(event.key==='Enter'&&!event.shiftKey&&!event.isComposing){event.preventDefault();els.messageForm.requestSubmit();}});
  document.querySelectorAll('[data-close-dialog]').forEach(button=>button.addEventListener('click',()=>els.agentDialog.close()));
  const sidebarToggle=$('sidebar-toggle');if(sidebarToggle)sidebarToggle.addEventListener('click',()=>document.querySelector('.sidebar').classList.toggle('open'));
}
window.addEventListener('DOMContentLoaded',async()=>{bindEvents();try{await bootstrap();}catch(error){showAuth();els.loginError.textContent=error.message||'服务启动中，请稍后重试';}});