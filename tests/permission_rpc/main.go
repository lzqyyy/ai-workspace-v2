package main

import (
    "context"
    "fmt"
    "os"
    "time"

    pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
    "google.golang.org/grpc"
    "google.golang.org/grpc/credentials/insecure"
)

type checkCase struct {
    name    string
    actor   string
    project string
    action  string
    allowed bool
}

func main() {
    addr := os.Getenv("GRPC_ADDR")
    if addr == "" {
        addr = "127.0.0.1:50051"
    }
    values := map[string]string{
        "owner": "", "editor": "", "agent": "",
        "editor_project": "", "viewer_project": "", "agentless_project": "",
    }
    for name := range values {
        key := "PERMISSION_" + upper(name)
        values[name] = os.Getenv(key)
        if values[name] == "" {
            panic("missing " + key)
        }
    }

    conn, err := grpc.NewClient(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
    if err != nil {
        panic(err)
    }
    defer conn.Close()
    client := pb.NewPermissionServiceClient(conn)
    cases := []checkCase{
        {"owner read", values["owner"], values["editor_project"], "agent.read", true},
        {"editor use", values["editor"], values["editor_project"], "agent.use", true},
        {"viewer read", values["editor"], values["viewer_project"], "agent.read", true},
        {"viewer cannot use", values["editor"], values["viewer_project"], "agent.use", false},
        {"agent outside project", values["editor"], values["agentless_project"], "agent.read", false},
        {"unknown action denied", values["owner"], values["editor_project"], "admin.everything", false},
    }
    for _, tc := range cases {
        ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
        response, callErr := client.CheckPermission(ctx, &pb.CheckPermissionRequest{
            Meta:         &pb.RequestMeta{RequestId: "permission-rpc-" + tc.name},
            ActorUserId:  tc.actor,
            ProjectId:    tc.project,
            ResourceType: pb.ResourceType_RESOURCE_TYPE_AGENT,
            ResourceId:   values["agent"],
            Action:       tc.action,
        })
        cancel()
        if callErr != nil {
            panic(fmt.Sprintf("%s: %v", tc.name, callErr))
        }
        if response.Allowed != tc.allowed {
            panic(fmt.Sprintf("%s: allowed=%v want=%v reason=%s", tc.name, response.Allowed, tc.allowed, response.Reason))
        }
    }
    fmt.Println(`{"status":"PASS","action_aware":true,"agent_project_context":true}`)
}

func upper(value string) string {
    out := make([]byte, len(value))
    for i := range value {
        ch := value[i]
        if ch >= 'a' && ch <= 'z' {
            ch -= 'a' - 'A'
        }
        out[i] = ch
    }
    return string(out)
}
