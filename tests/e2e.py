from __future__ import annotations
import http.cookiejar, json, os, subprocess, sys, urllib.error, urllib.parse, urllib.request, uuid
from pathlib import Path
BASE=os.environ.get('BASE_URL','http://127.0.0.1:8080')
HELPER=Path(os.environ.get('DUPLICATE_HELPER','build/duplicate-callback.exe')).resolve()
TERMINAL={'message.completed','run.failed','run.cancelled'}
class ApiFailure(RuntimeError):
    def __init__(self,status,payload): super().__init__(f'HTTP {status}: {payload}');self.status=status;self.code=payload.get('code')
class Client:
    def __init__(self):
        self.cookies=http.cookiejar.CookieJar();self.opener=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.cookies))
    def request(self,method,path,body=None,headers=None,request_id=None):
        rid=request_id or f'e2e-{uuid.uuid4()}';data=None if body is None else json.dumps(body,ensure_ascii=False).encode();req=urllib.request.Request(BASE+path,data=data,method=method);req.add_header('X-Request-ID',rid);req.add_header('Accept','application/json')
        if data is not None: req.add_header('Content-Type','application/json')
        for key,value in (headers or {}).items(): req.add_header(key,value)
        try:
            with self.opener.open(req,timeout=20) as response:
                if response.status==204:return None
                return json.loads(response.read().decode())
        except urllib.error.HTTPError as error:
            payload=json.loads(error.read().decode() or '{}');raise ApiFailure(error.code,payload) from error
    def events(self,run_id,after=0):
        query=urllib.parse.urlencode({'request_id':f'e2e-sse-{uuid.uuid4()}','after':after});req=urllib.request.Request(f'{BASE}/api/v1/runs/{run_id}/events?{query}');req.add_header('Accept','text/event-stream');req.add_header('X-Request-ID',f'e2e-sse-{uuid.uuid4()}');events=[];event_name='';event_data=''
        with self.opener.open(req,timeout=90) as response:
            while True:
                raw=response.readline()
                if not raw:break
                line=raw.decode().rstrip('\r\n')
                if line.startswith('event:'):event_name=line[6:].strip()
                elif line.startswith('data:'):event_data=line[5:].strip()
                elif line=='' and event_data:
                    envelope=json.loads(event_data)
                    if event_name and envelope.get('type')!=event_name:raise AssertionError('SSE event name does not match envelope type')
                    events.append(envelope)
                    if envelope['type'] in TERMINAL:break
                    event_name='';event_data=''
        return events
def login(client,username='demo',password='demo1234',origin=None):
    headers={'Origin':origin} if origin else None
    result=client.request('POST','/api/v1/auth/login',{'username':username,'password':password},headers=headers)
    assert result['user']['username']==username
    assert 'token' not in result,'session token must never be exposed'
    return result['user']
    result=client.request('POST','/api/v1/auth/login',{'username':username,'password':password});assert result['user']['username']==username;assert 'token' not in result,'session token must never be exposed';return result['user']
def send(client,conversation_id,content,request_id):
    return client.request('POST',f'/api/v1/conversations/{conversation_id}/messages',{'content':content,'client_request_id':request_id},{'Idempotency-Key':request_id},request_id)
def assert_event_contract(events):
    assert events,'no SSE events received';seqs=[event['seq'] for event in events];assert seqs==list(range(1,len(events)+1)),f'event sequence invalid: {seqs}';types={event['type'] for event in events};assert {'run.started','message.delta','message.completed'}<=types,types;assert events[-1]['type'] in TERMINAL
def main():
    bad=Client()
    try:
        login(bad,password='demo123')
        raise AssertionError('7-character demo password was accepted')
    except ApiFailure as error:
        assert error.status==400 and error.code=='VALIDATION_FAILED'
    evil=Client()
    try:
        login(evil,origin='http://evil.example')
        raise AssertionError('unlisted origin was accepted')
    except ApiFailure as error:
        assert error.status==403 and error.code=='FORBIDDEN'
    demo=Client();user=login(demo,origin='http://localhost:8080')
    agent=demo.request('POST','/api/v1/agents',{'name':f'E2E Agent {uuid.uuid4().hex[:6]}','system_prompt':'Return a concise end-to-end response.'})
    conversation=demo.request('POST','/api/v1/conversations',{'agent_id':agent['id'],'title':'Automated E2E'});cid=conversation['id']
    first_request=f'e2e-send-{uuid.uuid4()}';sent=send(demo,cid,'请执行一次完整的端到端测试',first_request);assert sent['status'] in {'queued','running'};run_id=sent['run_id']
    events=demo.events(run_id);assert_event_contract(events);completed=events[-1];assert completed['type']=='message.completed',completed;final_content=completed['payload']['content'];assert final_content;assert '请执行一次完整的端到端测试' in final_content, 'current user prompt missing from model context'
    messages=demo.request('GET',f'/api/v1/conversations/{cid}/messages')['items'];assert len(messages)==2,messages;assert messages[0]['sender_type']=='user' and messages[0]['content']=='请执行一次完整的端到端测试';assert messages[1]['sender_type']=='agent' and messages[1]['content']==final_content;assert messages[1]['run_id']==run_id
    duplicate=send(demo,cid,'请执行一次完整的端到端测试',first_request);assert duplicate['duplicate'] is True;assert duplicate['run_id']==run_id;assert len(demo.request('GET',f'/api/v1/conversations/{cid}/messages')['items'])==2
    job_id=events[0]['payload']['job_id'];helper=subprocess.run([str(HELPER),'-job',job_id,'-run',run_id],capture_output=True,text=True,timeout=30,check=False);assert helper.returncode==0,helper.stderr;callbacks=[json.loads(line) for line in helper.stdout.splitlines() if line.strip()];assert len(callbacks)==2 and all(item['duplicate'] and item['accepted'] for item in callbacks);assert len(demo.request('GET',f'/api/v1/conversations/{cid}/messages')['items'])==2
    replay=demo.events(run_id,after=1);assert replay and replay[0]['seq']==2 and replay[-1]['type']=='message.completed'
    other=Client();login(other,'other','demo1234',origin='http://127.0.0.1:8080')
    try:
        other.request('GET',f'/api/v1/conversations/{cid}/messages');raise AssertionError('cross-user message read was allowed')
    except ApiFailure as error:assert error.status==403 and error.code=='FORBIDDEN'
    cancel_request=f'e2e-cancel-{uuid.uuid4()}';cancelled_send=send(demo,cid,'这条消息将被立即取消',cancel_request);cancel_run_id=cancelled_send['run_id'];demo.request('POST',f'/api/v1/runs/{cancel_run_id}/cancel');cancel_events=demo.events(cancel_run_id);assert cancel_events[-1]['type']=='run.cancelled',cancel_events[-1]
    after_cancel=demo.request('GET',f'/api/v1/conversations/{cid}/messages')['items'];assert not any(item['sender_type']=='agent' and item['run_id']==cancel_run_id for item in after_cancel)
    retried=demo.request('POST',f'/api/v1/runs/{cancel_run_id}/retry',headers={'Idempotency-Key':f'e2e-retry-{uuid.uuid4()}'});retry_events=demo.events(retried['run_id']);assert retry_events[-1]['type']=='message.completed',retry_events[-1];after_retry=demo.request('GET',f'/api/v1/conversations/{cid}/messages')['items'];assert any(item['sender_type']=='agent' and item['run_id']==retried['run_id'] for item in after_retry)
    print(json.dumps({'status':'PASS','user':user['username'],'conversation_id':cid,'first_run_id':run_id,'event_count':len(events),'duplicate_callback':'PASS','sse_replay':'PASS','cross_user_denied':'PASS','cancel_and_retry':'PASS'},ensure_ascii=False));return 0
if __name__=='__main__':sys.exit(main())