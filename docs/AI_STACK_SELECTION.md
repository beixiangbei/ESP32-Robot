# Agent and Cloud AI Selection

Status: shortlist, not a frozen vendor decision

## Recommendation

Build a small Python agent gateway on the HPT630 with FastAPI, provider adapter
interfaces and Docker Compose. Start with push-to-talk and one device. Avoid a
large multi-agent framework until the device protocol, cancellation and safety
behavior are stable.

Recommended first implementation:

- Runtime: Python 3.12, FastAPI, asyncio, Pydantic and SQLite.
- Device transport: HTTP plus WebSocket initially; evaluate MQTT when multiple
  devices or reliable queued events become necessary.
- Agent loop: one tool-calling conversation agent with an explicit state machine.
- Memory: SQLite for configuration/audit and optional PostgreSQL later; do not add
  a vector database until retrieval requirements are demonstrated.
- Deployment: Docker Compose, health checks, restart policy and local log rotation.
- Observability: request IDs, provider latency, cancellation, errors and estimated
  usage/cost per conversation.

## Agent framework shortlist

| Option | Use when | Position for this project |
|---|---|---|
| Custom tool loop | One robot, clear tools, maximum control | Recommended for P4 prototype |
| OpenAI Agents SDK | OpenAI models are selected and tracing/handoffs are useful | Strong optional implementation |
| LangGraph | Long-running resumable workflows and human approval become necessary | Re-evaluate after prototype |
| Home Assistant integration | Apartment automation becomes a primary requirement | Integrate as a tool/event source, not the robot core |

The first agent should expose a small tool set such as `robot_status`,
`robot_look`, `robot_display`, `robot_capture`, `robot_beep` and `robot_sleep`.
Compound actions are expanded by the gateway, while firmware remains responsible
for limits and emergency cancellation.

## Cloud provider strategy

Use separate adapters for:

```text
AsrProvider.transcribe_stream(...)
TtsProvider.synthesize_stream(...)
LlmProvider.respond(..., tools=...)
VisionProvider.describe(image, ...)
```

Initial providers to benchmark:

| Capability | Primary candidates | Notes |
|---|---|---|
| Chinese streaming ASR | Alibaba Cloud speech, Volcano Engine speech, Tencent Cloud speech | Test apartment latency and interruption behavior |
| Chinese streaming TTS | Volcano Engine speech, Alibaba Cloud speech, Tencent Cloud speech | Compare naturalness, first-audio latency and voice licensing |
| Tool-calling LLM | OpenAI API, Alibaba Bailian/Qwen, Volcano Ark/Doubao | Evaluate tool accuracy separately from chat style |
| Vision | Same LLM provider where practical | Avoid a separate service until camera tasks require it |

Do not select from marketing demos alone. Run a recorded evaluation set through
each candidate and preserve results in the repository without audio containing
private conversations.

## Suggested starting combinations

### China-network-first baseline

- ASR/TTS: choose the better of Volcano Engine and Alibaba Cloud after a latency
  test from the apartment network.
- LLM/vision: evaluate Qwen and Doubao tool calling first; retain OpenAI as a
  quality/reference candidate if network access and account setup are reliable.
- Agent: custom Python tool loop with provider-neutral interfaces.

This is the recommended first deployment because voice latency and service
reachability matter more than framework features for an always-on apartment robot.

### Fastest OpenAI-centered prototype

- Agent: OpenAI Agents SDK or Responses API based tool loop.
- LLM/vision: a current tool-capable multimodal OpenAI model selected at
  implementation time from official documentation.
- ASR/TTS: OpenAI speech APIs or Realtime API, subject to an apartment-network
  latency test.

This reduces integration count, but it should not be frozen until connectivity,
latency, billing and Chinese speech quality are measured in the deployment network.

## Evaluation dataset

Create 30-50 non-sensitive samples covering:

- quiet, fan noise, television noise and speech at different distances;
- short commands, corrections, interruption and wake-word false positives;
- dates, numbers, English product names and Chinese room-control phrases;
- tool calls that are valid, out of range, ambiguous or physically unsafe;
- TTS responses from one sentence to a 30-second answer.

Metrics:

| Area | Required metrics |
|---|---|
| ASR | first partial latency, final latency, character error rate, disconnect recovery |
| TTS | first audio latency, naturalness score, cancellation latency, audio format support |
| LLM | correct tool/arguments, unsafe-call rejection, total latency, cost per task |
| Operations | availability, retry behavior, SDK quality, quota visibility, data region |

## Decision gates

A provider can be selected for the first release only when:

- apartment-network tests are recorded on at least two different days;
- streaming cancellation and timeout behavior is verified;
- expected monthly cost has been calculated from measured usage;
- credentials can be scoped and rotated;
- data retention and voice licensing terms have been reviewed;
- a fallback provider or degraded local behavior is documented.

## Information needed before freezing selection

- HPT630 CPU architecture, RAM, OS and available storage.
- Apartment uplink stability and whether overseas endpoints are consistently
  reachable.
- Expected daily conversation minutes and acceptable monthly budget.
- Whether the robot may send camera images to cloud services.
- Preferred voice style and whether commercial voice use is expected.

Model names, prices, regional availability and service terms change frequently.
Verify them against official provider documentation when implementation begins;
do not encode a time-sensitive model name into the firmware roadmap.

