package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	pb "ai-workspace-v2/worker/gen/aiworkspace/v1"
)

type Provider interface {
	Stream(context.Context, *pb.GetExecutionContextResponse, func(string) error) (json.RawMessage, error)
}

type modelError struct {
	Code string
	Err  error
}

func (e *modelError) Error() string { return e.Code + ": " + e.Err.Error() }
func (e *modelError) Unwrap() error { return e.Err }

type mockProvider struct{ delay time.Duration }

func (p mockProvider) Stream(ctx context.Context, exec *pb.GetExecutionContextResponse, onDelta func(string) error) (json.RawMessage, error) {
	prompt := ""
	for i := len(exec.Messages) - 1; i >= 0; i-- {
		if exec.Messages[i].Role == "user" {
			prompt = exec.Messages[i].Content
			break
		}
	}
	if len([]rune(prompt)) > 80 {
		prompt = string([]rune(prompt)[:80]) + "..."
	}
	reply := "你好！我已经通过 C++ 核心和 Go Worker 收到你的消息"
	if prompt != "" {
		reply += "：「" + prompt + "」"
	}
	reply += "。这是 Mock 模型返回的流式回复，用于验证 REST、gRPC、SSE 和 PostgreSQL 的完整链路。"
	runes := []rune(reply)
	const chunkSize = 7
	for start := 0; start < len(runes); start += chunkSize {
		end := start + chunkSize
		if end > len(runes) {
			end = len(runes)
		}
		select {
		case <-ctx.Done():
			return nil, &modelError{Code: "RUN_CANCELLED", Err: ctx.Err()}
		case <-time.After(p.delay):
		}
		if err := onDelta(string(runes[start:end])); err != nil {
			return nil, err
		}
	}
	return json.RawMessage(`{"input_tokens":0,"output_tokens":0,"provider":"mock"}`), nil
}

type openAIProvider struct {
	client  *http.Client
	apiKey  string
	baseURL string
}

func newOpenAIProvider(exec *pb.GetExecutionContextResponse, timeout time.Duration) (*openAIProvider, error) {
	if !strings.HasPrefix(exec.BaseUrl, "https://") && os.Getenv("ALLOW_INSECURE_MODEL_URL") != "true" {
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: errors.New("model base URL must use HTTPS")}
	}
	key := ""
	if exec.SecretRef != "" {
		key = os.Getenv(exec.SecretRef)
	}
	if key == "" {
		key = os.Getenv("MODEL_API_KEY")
	}
	if key == "" {
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: errors.New("model API key is not configured")}
	}
	return &openAIProvider{client: &http.Client{Timeout: timeout}, apiKey: key, baseURL: strings.TrimRight(exec.BaseUrl, "/")}, nil
}

func (p *openAIProvider) Stream(ctx context.Context, exec *pb.GetExecutionContextResponse, onDelta func(string) error) (json.RawMessage, error) {
	type message struct {
		Role    string `json:"role"`
		Content string `json:"content"`
	}
	messages := make([]message, 0, len(exec.Messages)+1)
	if exec.SystemPrompt != "" {
		messages = append(messages, message{Role: "system", Content: exec.SystemPrompt})
	}
	for _, item := range exec.Messages {
		if item.Role == "user" || item.Role == "assistant" {
			messages = append(messages, message{Role: item.Role, Content: item.Content})
		}
	}
	endpoint := p.baseURL
	if strings.HasSuffix(endpoint, "/v1") {
		endpoint += "/chat/completions"
	} else if !strings.HasSuffix(endpoint, "/chat/completions") {
		endpoint += "/v1/chat/completions"
	}
	body, _ := json.Marshal(map[string]any{"model": exec.ModelName, "messages": messages, "stream": true})
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, endpoint, bytes.NewReader(body))
	if err != nil {
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: err}
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+p.apiKey)
	resp, err := p.client.Do(req)
	if err != nil {
		if errors.Is(err, context.DeadlineExceeded) {
			return nil, &modelError{Code: "MODEL_TIMEOUT", Err: err}
		}
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: err}
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		_, _ = io.Copy(io.Discard, io.LimitReader(resp.Body, 4096))
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: fmt.Errorf("provider returned HTTP %d", resp.StatusCode)}
	}
	scanner := bufio.NewScanner(resp.Body)
	scanner.Buffer(make([]byte, 4096), 1<<20)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if !strings.HasPrefix(line, "data:") {
			continue
		}
		data := strings.TrimSpace(strings.TrimPrefix(line, "data:"))
		if data == "[DONE]" {
			break
		}
		var event struct {
			Choices []struct {
				Delta struct {
					Content string `json:"content"`
				} `json:"delta"`
			} `json:"choices"`
			Usage json.RawMessage `json:"usage"`
		}
		if err := json.Unmarshal([]byte(data), &event); err != nil {
			continue
		}
		if len(event.Choices) > 0 && event.Choices[0].Delta.Content != "" {
			if err := onDelta(event.Choices[0].Delta.Content); err != nil {
				return nil, err
			}
		}
	}
	if err := scanner.Err(); err != nil {
		if errors.Is(err, context.DeadlineExceeded) {
			return nil, &modelError{Code: "MODEL_TIMEOUT", Err: err}
		}
		if errors.Is(ctx.Err(), context.Canceled) {
			return nil, &modelError{Code: "RUN_CANCELLED", Err: ctx.Err()}
		}
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: err}
	}
	return json.RawMessage(`{}`), nil
}

func providerFor(exec *pb.GetExecutionContextResponse, cfg config) (Provider, error) {
	switch exec.ProviderType {
	case "mock":
		return mockProvider{delay: cfg.mockDelay}, nil
	case "openai_compatible":
		return newOpenAIProvider(exec, cfg.modelTimeout)
	default:
		return nil, &modelError{Code: "MODEL_PROVIDER_ERROR", Err: fmt.Errorf("unsupported provider type")}
	}
}
