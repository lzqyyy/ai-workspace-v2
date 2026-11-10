package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync/atomic"
	"syscall"
	"time"

	pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
)

type config struct {
	grpcAddr          string
	workerID          string
	pollInterval      time.Duration
	heartbeatInterval time.Duration
	modelTimeout      time.Duration
	mockDelay         time.Duration
}

var requestCounter atomic.Uint64
var errStreamCancelled = errors.New("run cancelled")

func envDuration(name string, fallback time.Duration) time.Duration {
	raw := os.Getenv(name)
	if raw == "" {
		return fallback
	}
	if n, err := strconv.Atoi(raw); err == nil {
		return time.Duration(n) * time.Millisecond
	}
	if d, err := time.ParseDuration(raw); err == nil {
		return d
	}
	return fallback
}

func loadConfig() config {
	host, _ := os.Hostname()
	workerID := os.Getenv("WORKER_ID")
	if workerID == "" {
		workerID = fmt.Sprintf("%s-%d", host, os.Getpid())
	}
	return config{grpcAddr: os.Getenv("GRPC_ADDR"), workerID: workerID,
		pollInterval:      envDuration("POLL_INTERVAL_MS", 750*time.Millisecond),
		heartbeatInterval: envDuration("HEARTBEAT_INTERVAL_MS", 2*time.Second),
		modelTimeout:      envDuration("MODEL_TIMEOUT_MS", 120*time.Second),
		mockDelay:         envDuration("MOCK_DELAY_MS", 250*time.Millisecond)}
}

type worker struct {
	cfg    config
	jobs   pb.JobServiceClient
	ctxsvc pb.ContextServiceClient
	events pb.RunEventServiceClient
}

func requestID() string {
	return fmt.Sprintf("worker-%d-%d", time.Now().UnixNano(), requestCounter.Add(1))
}
func meta() *pb.RequestMeta { return &pb.RequestMeta{RequestId: requestID()} }

func main() {
	log.SetFlags(log.LstdFlags | log.LUTC)
	cfg := loadConfig()
	if cfg.grpcAddr == "" {
		cfg.grpcAddr = "127.0.0.1:50051"
	}
	conn, err := grpc.NewClient(cfg.grpcAddr, grpc.WithTransportCredentials(insecure.NewCredentials()), grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(4<<20)))
	if err != nil {
		log.Fatalf("event=worker.start error=%q", err)
	}
	defer conn.Close()
	w := &worker{cfg: cfg, jobs: pb.NewJobServiceClient(conn), ctxsvc: pb.NewContextServiceClient(conn), events: pb.NewRunEventServiceClient(conn)}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	log.Printf("event=worker.started worker_id=%s grpc_addr=%s", cfg.workerID, cfg.grpcAddr)
	w.run(ctx)
	log.Printf("event=worker.stopped worker_id=%s", cfg.workerID)
}

func (w *worker) run(ctx context.Context) {
	for ctx.Err() == nil {
		callCtx, cancel := context.WithTimeout(ctx, 15*time.Second)
		response, err := w.jobs.ClaimNextJob(callCtx, &pb.ClaimNextJobRequest{Meta: meta(), WorkerId: w.cfg.workerID, AvailableSlots: 1})
		cancel()
		if err != nil {
			if ctx.Err() == nil {
				log.Printf("event=job.claim.error error=%q", err)
			}
			sleep(ctx, w.cfg.pollInterval)
			continue
		}
		if !response.JobAvailable || response.Job == nil {
			delay := time.Duration(response.PollAfterMs) * time.Millisecond
			if delay < 250*time.Millisecond {
				delay = w.cfg.pollInterval
			}
			sleep(ctx, delay)
			continue
		}
		if err := w.execute(ctx, response.Job); err != nil && ctx.Err() == nil {
			log.Printf("event=job.execute.error run_id=%s job_id=%s error=%q", response.Job.RunId, response.Job.JobId, err)
		}
	}
}

func sleep(ctx context.Context, duration time.Duration) {
	timer := time.NewTimer(duration)
	defer timer.Stop()
	select {
	case <-ctx.Done():
	case <-timer.C:
	}
}

func (w *worker) execute(parent context.Context, job *pb.JobLease) error {
	ctx, cancel := context.WithCancel(parent)
	defer cancel()
	callCtx, callCancel := context.WithTimeout(ctx, 15*time.Second)
	execution, err := w.ctxsvc.GetExecutionContext(callCtx, &pb.GetExecutionContextRequest{Meta: meta(), JobId: job.JobId, RunId: job.RunId, LeaseToken: job.LeaseToken})
	callCancel()
	if err != nil {
		return err
	}
	var cancelRequested atomic.Bool
	heartbeatDone := make(chan struct{})
	go w.heartbeat(ctx, job, &cancelRequested, cancel, heartbeatDone)
	defer func() { cancel(); <-heartbeatDone }()

	provider, err := providerFor(execution, w.cfg)
	if err != nil {
		return w.finish(job, "failed", errorCode(err), "", "")
	}
	seq := execution.NextEventSeq
	var content strings.Builder
	modelCtx, modelCancel := context.WithTimeout(ctx, w.cfg.modelTimeout)
	defer modelCancel()
	onDelta := func(delta string) error {
		if delta == "" {
			return nil
		}
		payload, _ := json.Marshal(map[string]string{"delta": delta})
		eventCtx, eventCancel := context.WithTimeout(ctx, 10*time.Second)
		_, appendErr := w.events.AppendEvent(eventCtx, &pb.AppendEventRequest{Meta: meta(), JobId: job.JobId, RunId: job.RunId, LeaseToken: job.LeaseToken, EventSeq: seq, EventType: "message.delta", PayloadJson: string(payload)})
		eventCancel()
		if appendErr != nil {
			if status.Code(appendErr) == codes.FailedPrecondition && (strings.Contains(appendErr.Error(), "RUN_CANCELLED") || strings.Contains(appendErr.Error(), "JOB_LEASE_INVALID")) {
				cancelRequested.Store(true)
				cancel()
				return errStreamCancelled
			}
			return appendErr
		}
		seq++
		content.WriteString(delta)
		return nil
	}
	usage, streamErr := provider.Stream(modelCtx, execution, onDelta)
	if cancelRequested.Load() || errors.Is(streamErr, errStreamCancelled) || (errors.Is(modelCtx.Err(), context.Canceled) && parent.Err() == nil) {
		return w.finish(job, "cancelled", "RUN_CANCELLED", "", "")
	}
	if streamErr != nil {
		if ctx.Err() != nil {
			return ctx.Err()
		}
		return w.finish(job, "failed", errorCode(streamErr), "", "")
	}
	if strings.TrimSpace(content.String()) == "" {
		return w.finish(job, "failed", "MODEL_PROVIDER_ERROR", "", "")
	}
	if len(usage) == 0 {
		usage = json.RawMessage(`{}`)
	}
	return w.finish(job, "succeeded", "", content.String(), string(usage))
}
func (w *worker) heartbeat(ctx context.Context, job *pb.JobLease, cancelRequested *atomic.Bool, cancel context.CancelFunc, done chan<- struct{}) {
	defer close(done)
	ticker := time.NewTicker(w.cfg.heartbeatInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			callCtx, callCancel := context.WithTimeout(ctx, 5*time.Second)
			response, err := w.jobs.Heartbeat(callCtx, &pb.HeartbeatRequest{Meta: meta(), JobId: job.JobId, RunId: job.RunId, LeaseToken: job.LeaseToken, LeaseSeconds: 20})
			callCancel()
			if err != nil || !response.LeaseValid || response.CancelRequested {
				cancelRequested.Store(true)
				cancel()
				return
			}
		}
	}
}

func (w *worker) finish(job *pb.JobLease, state, errorCode, content, usage string) error {
	statusValue := pb.JobStatus_JOB_STATUS_FAILED
	if state == "succeeded" {
		statusValue = pb.JobStatus_JOB_STATUS_SUCCEEDED
	}
	if state == "cancelled" {
		statusValue = pb.JobStatus_JOB_STATUS_CANCELLED
	}
	callCtx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	response, err := w.jobs.UpdateJobStatus(callCtx, &pb.UpdateJobStatusRequest{Meta: meta(), JobId: job.JobId, RunId: job.RunId,
		LeaseToken: job.LeaseToken, Status: statusValue, ErrorCode: errorCode, FinalContent: content, UsageJson: usage})
	if err != nil {
		return err
	}
	log.Printf("event=job.finished run_id=%s job_id=%s status=%s duplicate=%t final_seq=%d", job.RunId, job.JobId, state, response.Duplicate, response.FinalEventSeq)
	return nil
}

func errorCode(err error) string {
	var modelErr *modelError
	if errors.As(err, &modelErr) {
		return modelErr.Code
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return "MODEL_TIMEOUT"
	}
	if errors.Is(err, context.Canceled) {
		return "RUN_CANCELLED"
	}
	return "MODEL_PROVIDER_ERROR"
}
