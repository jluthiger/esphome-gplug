---
name: api-change
description: Add or change a gPlug device HTTP API route or JSON field consistently across firmware handler, SPA client, mock server and both READMEs. Use whenever /api/* request or response shape changes.
---

# API change

The device API exists in five places. Change all of them in one go:

| # | Where | What |
|---|---|---|
| 1 | `firmware/components/gplug_smi/gplug_smi.cpp` (+ `.h`) | handler; GET/POST only; JSON body read by the handler; fixed-size buffers, no unbounded `std::string` growth on large responses (stream like `/api/history.csv`) |
| 2 | `spa/src/api.js` and the consuming component | client call, tolerate the field being absent (older firmware may be running: see how `detect` is handled in `steps/meter.js`) |
| 3 | `spa/mock/server.mjs` | same shape as the device; add a `MOCK_*` env var if the new state is hard to reach |
| 4 | `firmware/README.md` → "HTTP API" table | firmware view |
| 5 | `spa/README.md` → "Device API used" table | SPA view |

Steps:

1. Grep the route/field name across the repo first to find every existing use.
2. If logic is decode/storage work, put it in a header-only file with a host test (`firmware/test/`).
3. Edit 1–5. New user-visible text → all four `spa/src/i18n/*.js`.
4. Run `/verify` (build, tests, `esphome compile dev.yaml`; response size matters on a 400 kB SRAM device).
5. Check the mock and firmware agree: diff the mock's JSON against the handler's field list by eye
   and list any field present in only one.
6. Never expose the GUEK / auth key in any response.
