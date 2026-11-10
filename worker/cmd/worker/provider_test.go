package main

import (
	"context"
	"strings"
	"testing"
	"time"

	pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
)

func TestMockProviderStreamsAndPreservesPrompt(t *testing.T) {
	exec := &pb.GetExecutionContextResponse{Messages: []*pb.ConversationMessage{{Role: "user", Content: "hello"}}}
	var content strings.Builder
	chunks := 0
	usage, err := (mockProvider{delay: time.Millisecond}).Stream(context.Background(), exec, func(delta string) error {
		chunks++
		content.WriteString(delta)
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if chunks < 2 {
		t.Fatalf("expected multiple chunks, got %d", chunks)
	}
	if !strings.Contains(content.String(), "hello") {
		t.Fatalf("prompt missing: %s", content.String())
	}
	if !strings.Contains(string(usage), "mock") {
		t.Fatalf("usage: %s", usage)
	}
}

func TestMockProviderHonoursCancellation(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err := (mockProvider{delay: time.Second}).Stream(ctx, &pb.GetExecutionContextResponse{}, func(string) error { return nil })
	if err == nil {
		t.Fatal("expected cancellation error")
	}
}
