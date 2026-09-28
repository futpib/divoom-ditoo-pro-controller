# Live Ditoo Pro evidence

Captured over BLE on firmware 306007, 2026-09-28. Files are JSON/JSONL CLI
results, not assertions that untested commands work. Earlier captures predate
the added delay/listen fields in request descriptions.

- `device-info.jsonl`: firmware, display state and volume.
- `device-settings.jsonl`: 12/24-hour, saved volume and auto-connect queries.
- `queries.jsonl`: alarm query success and individual query timeouts, with stderr.
- `query-events.jsonl`: tool/key query transport ACKs without responses; SD reply.
- `readback.json`: separate-process clock setting writes acknowledged, readbacks
  timed out; includes the restoring write.
- `final-capture.jsonl`: confirms restored clock setting and brightness zero,
  SD reply and an unsolicited F7 packet.
- `readback-batch.jsonl`: successful clock change, query, restore, query, SD
  status and final brightness-zero query in one connection. The accompanying
  requests file describes this mutating test, not a default startup script.

First attempt at the same readback batch stopped at its initial firmware query
with `Timed out waiting for BLE acknowledgment/response`; no settings were
written in that attempt. The successful batch allowed event capture for that
initial request. Cause of intermittent standalone query timeouts is unresolved.
No firmware, reset, game or alarm writes were performed.
