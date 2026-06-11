# piscine — 4-channel Zigbee peristaltic pump controller

nRF52840 (nice!nano v2-compatible) + 4× L298N-mini, NCS/Zephyr + ZBOSS, zigbee2mqtt + Home Assistant.

## Source of truth

`specs/` is normative. Code must conform to it; if code and spec disagree, fix the code or update the spec first. Never invent pin numbers, cluster/attribute IDs, or function signatures — they are fixed in:

- `specs/01-hardware.md` — pin map, PWM frequency, drive scheme
- `specs/02-zigbee-model.md` — endpoints, clusters, attribute/command IDs
- `specs/03-firmware-modules.md` — module boundaries and exact C signatures
- `specs/04-z2m-converter.md` — converter contract

Safety invariants in `specs/00-overview.md` are non-negotiable (pumps stopped on boot/error, doses bounded at 3600 s).

## Conventions

- C, Zephyr style: tabs, `LOG_MODULE_REGISTER(<module>, LOG_LEVEL_INF)`, return 0/-errno.
- Strict layering: pump_pwm (HW) ← dosing (timing) ← zb_pump (Zigbee). No upward includes.
- Never call ZBOSS APIs from workqueue/ISR context; schedule onto the Zigbee thread.
- Build: `west build -b nice_nano_v2 firmware`. Flash: `west flash` (SWD probe). Logs: RTT.

## Delegation rules (Haiku coding agents)

Tasks live in `tasks/`; `tasks/TASKS.md` defines order and dependencies. Orchestrator (Fable/Opus/Sonnet session) dispatches each task to a Haiku agent and reviews the result.

1. **One task = one agent = one module.** Dispatch in dependency order; do not start a task before its dependencies are reviewed.
2. **Briefs are self-contained.** Each `tasks/task-XX-*.md` inlines everything Haiku needs (signatures, IDs, file paths, done-criteria). Haiku reads its brief + the spec files it names — nothing else is assumed.
3. **No invention.** Haiku must copy pins/IDs/signatures verbatim from the brief. If anything is ambiguous, missing, or contradictory, Haiku stops and reports the question instead of guessing.
4. **Done-criteria are mandatory.** A task is complete only when its brief's checklist passes (build compiles, `node --check`, tests). Haiku reports exactly which criteria were verified and how.
5. **Review gate.** After each Haiku task, the orchestrator: builds, diffs against the spec (IDs, signatures, pin map), runs the per-task gate in `specs/05-test-plan.md`. Failures go back to the same Haiku agent (via SendMessage) with the concrete error, not a re-explanation of the architecture.
6. **Scope discipline.** Haiku touches only the files its brief lists. No drive-by refactors, no editing specs, no editing other modules' files.

Dispatch template:

```
Agent(subagent_type: general-purpose, model: haiku, prompt:
  "Read tasks/task-XX-<name>.md (relative to the repo root) and execute it exactly.
   Follow CLAUDE.md delegation rules 3, 4, 6. Report done-criteria results.")
```
