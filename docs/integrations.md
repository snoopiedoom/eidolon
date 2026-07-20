# Conversation providers and session integration

Eidolon is not a Codex client. Provider adapters translate vendor-specific transports into one
small conversation contract; the session registry, dialogue engine, affect system, and renderers
never parse vendor payloads.

## Normalized contract

Every provider event carries a provider id, session id, optional turn/message ids, title, UTF-8
text, phase, and source timestamp. The event kinds are source connection changes, session metadata,
turn start/completion, text delta, and message completion.

Session identity is the pair `(provider, session id)`. File paths, titles, transport connections,
and bubble slots are mutable metadata. Two vendors may use the same session id without collision.

Provider parsers live in `src/providers/*_stream.c`. Blocking transports live behind
`src/providers/live_source.c` and publish into the bounded thread-safe event bus. Adding a provider
requires a parser, a transport start function when a supported live transport exists, one provider
catalog entry, configuration keys, fixtures, and this capability table. It must not add vendor
branches to the renderer or dialogue engine.

## Capability state

- **Codex** — live CLI lifecycle, deltas, snapshots, titles, and multiple sessions through an
  in-path WebSocket/stdin relay; completion and history remain available through the independent
  transcript fallback. A passive app-server client remains available for protocol development.
- **OpenCode** — live lifecycle, deltas, completion snapshots, titles, and multiple sessions
  through the server's SSE event stream.
- **ChatGPT Desktop chat** — reserved adapter, currently unavailable. Ordinary Chat has no
  documented local conversation stream. The Codex surface inside the combined desktop app belongs
  to the Codex adapter, not a ChatGPT-specific transport.
- **ZCode** — reserved adapter, currently unavailable. Its installed protocol must be inspected
  before capabilities are claimed.

Unavailable is a real state, not an invitation to scrape windows, inject code, or read another
process's private pipe. An enabled adapter without a supported transport reports unavailable.

## Configuration

Shipped provider configuration lives in `config/providers.cfg`. Live transports are opt-in so a
normal Eidolon launch does not create noisy connection retries when no shared provider server is
running.

```text
version = 1

codex.live.enabled = false
codex.live.url = ws://127.0.0.1:4500

codex.relay.enabled = false
codex.relay.listen = ws://127.0.0.1:4500
codex.relay.executable = codex

opencode.live.enabled = false
opencode.live.url = http://127.0.0.1:4096/event

chatgpt.live.enabled = false
zcode.live.enabled = false

legacy.codex_transcripts.enabled = true
legacy.hooks.enabled = true
```

Unknown keys are ignored for forward compatibility. Known keys reject invalid values rather than
silently changing behavior.

## Codex CLI live relay

Codex subscriptions are connection-scoped, so Eidolon must sit in the traffic path to observe the
same turn as the TUI. Enable the relay, start Eidolon, then point the TUI at it:

```text
codex.relay.enabled = true
codex.relay.listen = ws://127.0.0.1:4500
codex.relay.executable = codex
```

```powershell
./build/windows/eidolon.exe
codex --remote ws://127.0.0.1:4500
```

Eidolon accepts one localhost WebSocket client at a time and launches a hidden
`codex app-server --listen stdio://` child for that client. Client frames, including approvals and
cancellation, pass to the child unchanged. Server frames pass back unchanged and are also observed
read-only by the Codex parser. A one-megabyte message bound prevents unbounded relay allocation;
disconnecting either side interrupts the other and ends the owned child.

`codex.relay.enabled` takes precedence over `codex.live.enabled`. Transcript and hook fallbacks
remain independent and may stay enabled. The registry merges relay and transcript updates by the
same `(codex, thread id)` identity rather than creating duplicate bubbles.

Verify the complete hidden transport without opening a TUI:

```powershell
make codex-relay-test
```

This exercises the localhost upgrade, WebSocket framing in both directions, child process launch,
JSONL forwarding, Codex initialization response, and teardown against the installed CLI.

## Codex passive app-server boundary

The experimental client can connect to an app-server for protocol development:

```powershell
codex app-server --listen ws://127.0.0.1:4500
```

Then set `codex.live.enabled = true`. Eidolon initializes its own connection and can parse
`item/agentMessage/delta` plus completion and lifecycle notifications delivered to that connection.

This does **not** attach Eidolon to a TUI or Desktop turn owned by a different connection. App-server
subscriptions are connection-scoped, and stock Codex does not currently promise reliable live
peer-client co-presence. Connecting the TUI and Eidolon separately to one listener is therefore not
a supported streaming topology.

References: [official app-server protocol](https://github.com/openai/codex/blob/main/codex-rs/app-server/README.md)
and the [peer-client co-presence architecture discussion](https://github.com/openai/codex/issues/21551).

The combined ChatGPT desktop app normally launches a private Codex app-server over inherited
stdio. A second process cannot safely become another reader on that private pipe. Its supported
Eidolon path remains the completed-response transcript fallback.

Probe the protocol handshake without launching the visible overlay:

```powershell
make provider-live-test PROVIDER=codex PROVIDER_URL=ws://127.0.0.1:4500
```

## OpenCode live setup

Run the headless server, attach the TUI to it, and enable the matching Eidolon URL:

```powershell
opencode serve --port 4096
opencode attach http://127.0.0.1:4096
```

```text
opencode.live.enabled = true
opencode.live.url = http://127.0.0.1:4096/event
```

The adapter accepts both current `session.next.text.*` events and the compatible
`message.part.delta` form. If `OPENCODE_SERVER_PASSWORD` is configured, Eidolon uses HTTP Basic
credentials with `OPENCODE_SERVER_USERNAME` or `opencode` as the default username.

Reference: [OpenCode server API](https://opencode.ai/docs/server/).

Probe the stream with:

```powershell
make provider-live-test PROVIDER=opencode PROVIDER_URL=http://127.0.0.1:4096/event
```

## Streaming and recovery

The first text delta makes its session visible immediately. Subsequent deltas extend the existing
UTF-8 dialogue and preserve the reveal cursor; they do not restart the message or expression track.
The completed-message snapshot is authoritative and repairs a missed event-bus delta. Queue
overflow rejects a delta and increments a counter instead of corrupting order.

Live text begins with a generic responding expression. Full-message sources still use the prepared
semantic-beat planner. Incremental beat classification synchronized to streamed sentence
boundaries remains the next expression-system step.

## Legacy Codex accessors

Transcript discovery under `~/.codex/sessions` and hook IPC remain independent switches. Discovery
runs asynchronously and scans the full tree every five seconds while cheap stamp checks keep known
files current. A live Codex thread and its transcript resolve to the same `(codex, thread id)` entry,
so the fallback cannot create a duplicate bubble.

The hook command remains best-effort and cosmetic:

```powershell
./build/windows/eidolon.exe --hook running
./build/windows/eidolon.exe --hook waiting
./build/windows/eidolon.exe --hook review
```

Sample definitions live in `hooks/codex-hooks.windows.json` and `hooks/codex-hooks.json`. Existing
hook configuration must be merged, not overwritten. A provider failure never fails an agent turn.
