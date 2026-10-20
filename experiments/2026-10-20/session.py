"""Cost, time and timeline of one Claude Code session, from its transcript.

    python3 session.py <transcript.jsonl>     # prints usage, writes figures/timeline.png beside it
"""

import collections
import datetime
import json
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt  # pylint: disable=wrong-import-position

# USD per million tokens, Claude Opus 5.5 (claude-api skill, cached 2026-10-06)
PRICE = {"input": 4.0, "write_5m": 5.0, "write_1h": 8.0, "read": 0.20, "output": 20.0}
LOCAL = datetime.timezone(datetime.timedelta(hours=-5))  # CDT, as the run files' mtimes
AWAY = 300  # s; a longer pause by the scientist is waiting, not working
COLORS = {"Scientist": "#eb6834", "Agent": "#2a78d6", "Compute": "#1baf7a"}


def stamp(entry):
    """Local datetime of a transcript entry."""
    return datetime.datetime.fromisoformat(
        entry["timestamp"].replace("Z", "+00:00")
    ).astimezone(LOCAL)


def load(path):
    """Usage per API call, prompts, agent triggers, tool spans of docker runs."""
    calls, prompts, triggers, replies, compute = {}, [], [], [], []
    started, detached = {}, None
    for line in open(path, encoding="utf-8"):
        entry = json.loads(line)
        if "timestamp" not in entry or entry.get("type") not in ("user", "assistant"):
            continue
        time, content = stamp(entry), entry["message"].get("content")
        if entry["type"] == "assistant":
            calls[entry["message"]["id"]] = entry["message"].get("usage", {})
            replies.append(time)
            for block in content:
                command = block.get("input", {}).get("command", "")
                if block.get("type") != "tool_use" or "docker" not in command:
                    continue
                if "docker stop" in command and detached:
                    compute.append((detached, time))
                    detached = None
                if "docker run -d" in command:
                    detached = time
                elif "docker run" in command or "docker wait" in command:
                    started[block["id"]] = time
        elif isinstance(content, str):
            triggers.append(time)
            if detached and content.startswith("<task-notification"):
                compute.append((detached, time))  # background `docker wait` returned
                detached = None
            if not content.startswith("<"):
                prompts.append((time, content))
        else:
            for block in content:
                if block.get("tool_use_id") in started:
                    compute.append((started[block["tool_use_id"]], time))
    return calls, prompts, triggers, replies, compute


def spans(triggers, replies):
    """Agent working spans: each trigger to its last reply before the next trigger."""
    out = []
    for k, start in enumerate(triggers):
        stop = triggers[k + 1] if k + 1 < len(triggers) else max(replies)
        last = max((r for r in replies if start <= r < stop), default=start)
        out.append((start, last))
    return out


def usage(calls):
    """Token totals by price class and their cost."""
    total = collections.Counter()
    for item in calls.values():
        split = item.get("cache_creation", {})
        total["input"] += item.get("input_tokens", 0)
        total["write_5m"] += split.get("ephemeral_5m_input_tokens", 0)
        total["write_1h"] += split.get("ephemeral_1h_input_tokens", 0)
        total["read"] += item.get("cache_read_input_tokens", 0)
        total["output"] += item.get("output_tokens", 0)
    return total, {key: total[key] * PRICE[key] / 1e6 for key in PRICE}


def plot(lanes, filename):
    """Three-lane timeline: who was busy when."""
    fig, axis = plt.subplots(figsize=(10, 2.4))
    for row, (name, bars) in enumerate(lanes.items()):
        for start, stop, faded in bars:
            axis.barh(
                row,
                (stop - start).total_seconds() / 86400,
                left=matplotlib.dates.date2num(start),
                height=0.6,
                color=COLORS[name],
                alpha=0.25 if faded else 1.0,
                linewidth=0,
            )
    axis.set_yticks(range(len(lanes)), list(lanes))
    axis.invert_yaxis()
    axis.xaxis.set_major_locator(
        matplotlib.dates.MinuteLocator(byminute=[0, 15, 30, 45], tz=LOCAL)
    )
    axis.xaxis.set_major_formatter(matplotlib.dates.DateFormatter("%H:%M", tz=LOCAL))
    axis.grid(axis="x", color="#e4e3df")
    axis.set_axisbelow(True)
    for side in ("top", "right", "left"):
        axis.spines[side].set_visible(False)
    axis.tick_params(left=False, colors="#52514e")
    fig.tight_layout()
    fig.savefig(filename, dpi=160)


def main(path):
    """Print the numbers of the report and draw its timeline."""
    calls, prompts, triggers, replies, compute = load(path)
    agent = spans(triggers, replies)
    asks = [t for t, _ in prompts]
    human = []
    for time in asks[1:]:
        last = max(r for r in replies if r < time)
        human.append((last, time, (time - last).total_seconds() > AWAY))
    total, cost = usage(calls)
    context = sum(total[k] for k in ("input", "write_5m", "write_1h", "read"))

    seconds = lambda bars: sum(
        (b[1] - b[0]).total_seconds() for b in bars
    )  # noqa: E731
    print(
        f"window {asks[0]:%H:%M}-{max(replies):%H:%M}, prompts {len(asks)}, "
        f"words {sum(len(c.split()) for _, c in prompts)}, api calls {len(calls)}"
    )
    print(
        f"agent {seconds(agent) / 60:.0f} min, scientist active "
        f"{seconds([h for h in human if not h[2]]) / 60:.0f} min, away "
        f"{seconds([h for h in human if h[2]]) / 60:.0f} min, docker {seconds(compute) / 60:.0f} min"
    )
    for key in PRICE:
        print(f"{key:9s} {total[key]:>10,d} tok  ${cost[key]:6.2f}")
    print(f"total ${sum(cost.values()):.2f}, cache hit {total['read'] / context:.1%}")

    plot(
        {
            "Scientist": human,
            "Agent": [(a, b, False) for a, b in agent],
            "Compute": [(a, b, False) for a, b in compute],
        },
        Path(__file__).resolve().parent / "figures/timeline.png",
    )


if __name__ == "__main__":
    main(sys.argv[1])
