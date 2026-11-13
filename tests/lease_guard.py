from __future__ import annotations
import json, uuid
from e2e import Client, callback_helper, login, send

def main():
    client=Client();login(client,origin='http://localhost:8080')
    agent=client.request('POST','/api/v1/agents',{'name':f'Lease Guard {uuid.uuid4().hex[:6]}','system_prompt':'Lease guard test.'})
    conversation=client.request('POST','/api/v1/conversations',{'agent_id':agent['id'],'title':'Lease guard'})
    sent=send(client,conversation['id'],'lease rejection test',f'e2e-lease-{uuid.uuid4()}')
    assert sent['status']=='queued',sent
    result=callback_helper(mode='reject')
    assert result and result[0]['code']=='JOB_LEASE_INVALID',result
    assert result[0]['run_id']==sent['run_id'],(result,sent)
    events=client.events(sent['run_id'])
    assert events[-1]['type']=='run.cancelled',events[-1]
    messages=client.request('GET',f"/api/v1/conversations/{conversation['id']}/messages")['items']
    assert len(messages)==1 and messages[0]['sender_type']=='user',messages
    print(json.dumps({'status':'PASS','wrong_lease':'JOB_LEASE_INVALID','run_status':'cancelled'},ensure_ascii=False))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
