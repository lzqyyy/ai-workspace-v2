package main

import (
    "context"
    "encoding/json"
    "flag"
    "fmt"
    "os"
    "time"

    pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
    "google.golang.org/grpc"
    "google.golang.org/grpc/credentials/insecure"
)

func main() {
    addr := flag.String("addr", "127.0.0.1:50051", "gRPC address")
    jobID := flag.String("job", "", "job id")
    runID := flag.String("run", "", "run id")
    flag.Parse()
    if *jobID == "" || *runID == "" { fmt.Fprintln(os.Stderr, "job and run are required"); os.Exit(2) }
    conn, err := grpc.NewClient(*addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
    if err != nil { panic(err) }
    defer conn.Close()
    client := pb.NewJobServiceClient(conn)
    request := &pb.UpdateJobStatusRequest{Meta: &pb.RequestMeta{RequestId: "e2e-duplicate-callback"},
        JobId: *jobID, RunId: *runID, LeaseToken: "already-cleared-lease", Status: pb.JobStatus_JOB_STATUS_SUCCEEDED,
        FinalContent: "duplicate callback must be ignored"}
    for i := 0; i < 2; i++ {
        ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
        response, callErr := client.UpdateJobStatus(ctx, request)
        cancel()
        if callErr != nil { fmt.Fprintln(os.Stderr, callErr); os.Exit(1) }
        output, _ := json.Marshal(map[string]any{"attempt": i + 1, "accepted": response.Accepted, "duplicate": response.Duplicate, "run_status": response.RunStatus.String()})
        fmt.Println(string(output))
        if !response.Accepted || !response.Duplicate { os.Exit(1) }
    }
}