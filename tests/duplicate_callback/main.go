package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"strings"
	"time"

	pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
)

func main() {
	defaultAddr := os.Getenv("GRPC_ADDR")
	if defaultAddr == "" {
		defaultAddr = "127.0.0.1:50051"
	}
	addr := flag.String("addr", defaultAddr, "gRPC address")
	jobID := flag.String("job", "", "job id")
	runID := flag.String("run", "", "run id")
	mode := flag.String("mode", "duplicate", "duplicate, reject, or claim")
	flag.Parse()

	conn, err := grpc.NewClient(*addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		panic(err)
	}
	defer conn.Close()
	client := pb.NewJobServiceClient(conn)

	if *mode == "reject" {
		rejectWrongLease(client, *jobID, *runID)
		return
	}
	if *mode == "claim" {
		job := claimNextJob(client)
		output, _ := json.Marshal(map[string]any{"job_id": job.JobId, "run_id": job.RunId, "lease_token": job.LeaseToken,
			"lease_expires_at_unix_ms": job.LeaseExpiresAtUnixMs, "attempt": job.Attempt, "max_attempts": job.MaxAttempts})
		fmt.Println(string(output))
		return
	}
	if *mode == "claim-once" {
		job, available := claimOnce(client)
		output := map[string]any{"job_available": available}
		if available {
			output["job_id"] = job.JobId
			output["run_id"] = job.RunId
			output["lease_token"] = job.LeaseToken
			output["lease_expires_at_unix_ms"] = job.LeaseExpiresAtUnixMs
		}
		encoded, _ := json.Marshal(output)
		fmt.Println(string(encoded))
		return
	}
	if *mode != "duplicate" {
		fmt.Fprintln(os.Stderr, "mode must be duplicate, reject, or claim")
		os.Exit(2)
	}
	if *jobID == "" || *runID == "" {
		fmt.Fprintln(os.Stderr, "job and run are required for duplicate mode")
		os.Exit(2)
	}
	request := &pb.UpdateJobStatusRequest{Meta: &pb.RequestMeta{RequestId: "e2e-duplicate-callback"},
		JobId: *jobID, RunId: *runID, LeaseToken: "already-cleared-lease", Status: pb.JobStatus_JOB_STATUS_SUCCEEDED,
		FinalContent: "duplicate callback must be ignored"}
	for i := 0; i < 2; i++ {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		response, callErr := client.UpdateJobStatus(ctx, request)
		cancel()
		if callErr != nil {
			fmt.Fprintln(os.Stderr, callErr)
			os.Exit(1)
		}
		output, _ := json.Marshal(map[string]any{"attempt": i + 1, "accepted": response.Accepted, "duplicate": response.Duplicate, "run_status": response.RunStatus.String()})
		fmt.Println(string(output))
		if !response.Accepted || !response.Duplicate {
			os.Exit(1)
		}
	}
}

func rejectWrongLease(client pb.JobServiceClient, jobID, runID string) {
	claimed := false
	leaseToken := "wrong-lease-token"
	if jobID == "" || runID == "" {
		job := claimNextJob(client)
		jobID, runID, leaseToken = job.JobId, job.RunId, job.LeaseToken
		claimed = true
	}

	wrong := &pb.UpdateJobStatusRequest{Meta: &pb.RequestMeta{RequestId: "e2e-wrong-lease"},
		JobId: jobID, RunId: runID, LeaseToken: "wrong-lease-token", Status: pb.JobStatus_JOB_STATUS_SUCCEEDED,
		FinalContent: "callback with the wrong lease must be rejected"}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	_, callErr := client.UpdateJobStatus(ctx, wrong)
	cancel()
	if status.Code(callErr) != codes.FailedPrecondition || !strings.Contains(callErr.Error(), "JOB_LEASE_INVALID") {
		fmt.Fprintf(os.Stderr, "expected JOB_LEASE_INVALID, got %v\n", callErr)
		os.Exit(1)
	}
	if claimed {
		finish := &pb.UpdateJobStatusRequest{Meta: &pb.RequestMeta{RequestId: "e2e-lease-test-cleanup"},
			JobId: jobID, RunId: runID, LeaseToken: leaseToken, Status: pb.JobStatus_JOB_STATUS_CANCELLED,
			ErrorCode: "RUN_CANCELLED"}
		ctx, cancel = context.WithTimeout(context.Background(), 10*time.Second)
		response, finishErr := client.UpdateJobStatus(ctx, finish)
		cancel()
		if finishErr != nil || !response.Accepted {
			fmt.Fprintf(os.Stderr, "failed to cancel lease-test job: %v\n", finishErr)
			os.Exit(1)
		}
	}
	output, _ := json.Marshal(map[string]any{"accepted": false, "code": "JOB_LEASE_INVALID", "job_id": jobID, "run_id": runID})
	fmt.Println(string(output))
}

func claimNextJob(client pb.JobServiceClient) *pb.JobLease {
	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) {
		job, available := claimOnce(client)
		if available {
			return job
		}
		time.Sleep(100 * time.Millisecond)
	}
	fmt.Fprintln(os.Stderr, "timed out waiting for a job to lease")
	os.Exit(1)
	return nil
}

func claimOnce(client pb.JobServiceClient) (*pb.JobLease, bool) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	response, err := client.ClaimNextJob(ctx, &pb.ClaimNextJobRequest{Meta: &pb.RequestMeta{RequestId: "e2e-lease-guard"},
		WorkerId: "e2e-lease-guard", AvailableSlots: 1})
	cancel()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	return response.Job, response.JobAvailable && response.Job != nil
}
