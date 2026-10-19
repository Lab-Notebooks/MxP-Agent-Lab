#!/usr/bin/env python3
"""Pure Antigravity + Argon Client & Evolutionary Runner (Zero Jetski Dependency).

Designed for external/portable reproduction on any machine or container with:
  1. Google Antigravity (`google-antigravity` Python SDK or `antigravity` / `agy` CLI)
  2. Argon model access (`gemini-launch-candidate-high`, `ARGON_API_KEY`, `GEMINI_API_KEY`, or Vertex AI ADC)
  3. ZERO dependency on internal `jetski` binaries, `~/.gemini/jetski` paths, or corp LOAS2 tools.
"""

from __future__ import annotations

import asyncio
import json
import os
import re
import shutil
import subprocess
import urllib.request
from pathlib import Path
from typing import Any

DEFAULT_ARGON_MODEL = os.getenv("ARGON_MODEL", "gemini-launch-candidate-high")


def load_dotenv_if_present(repo_root: Path) -> None:
    """Load .env from repo root or current working directory without external deps."""
    for env_path in (repo_root / ".env", Path.cwd() / ".env"):
        if env_path.is_file():
            for raw_line in env_path.read_text(encoding="utf-8").splitlines():
                line = raw_line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                key = k.strip()
                val = v.strip().strip('"').strip("'")
                if key and key not in os.environ:
                    os.environ[key] = val


class ArgonAntigravityClient:
    """3-tier Antigravity + Argon client with zero Jetski dependency."""

    def __init__(self, repo_root: Path, model: str | None = None, interactive: bool = True) -> None:
        self.repo_root = repo_root.resolve()
        load_dotenv_if_present(self.repo_root)
        self.model = model or os.getenv("ARGON_MODEL", DEFAULT_ARGON_MODEL)
        self.interactive = interactive

    def detect_backend(self) -> dict[str, Any]:
        """Inspect available Antigravity SDK, Antigravity CLI, and Argon credentials."""
        has_sdk = False
        try:
            import google.antigravity  # type: ignore  # noqa: F401
            has_sdk = True
        except ImportError:
            has_sdk = False

        agy_bin = shutil.which("antigravity") or shutil.which("agy")
        has_api_key = bool(os.getenv("ARGON_API_KEY") or os.getenv("GEMINI_API_KEY"))
        has_vertex = bool(os.getenv("GOOGLE_CLOUD_PROJECT"))
        return {
            "google_antigravity_sdk": has_sdk,
            "antigravity_cli_binary": agy_bin,
            "argon_model": self.model,
            "api_key_configured": has_api_key,
            "vertex_project": os.getenv("GOOGLE_CLOUD_PROJECT", ""),
            "jetski_dependency": False,
        }

    async def _ask_via_sdk(self, prompt: str, system_instruction: str = "") -> str:
        """Tier 1: Official Google Antigravity Python SDK (`google.antigravity`)."""
        from google.antigravity import Agent, LocalAgentConfig, types  # type: ignore

        api_key = os.getenv("ARGON_API_KEY") or os.getenv("GEMINI_API_KEY")
        vertex_project = os.getenv("GOOGLE_CLOUD_PROJECT")
        behavior = (
            types.AgentBehavior.INTERACTIVE
            if self.interactive
            else types.AgentBehavior.AUTONOMOUS
        )
        cfg_kwargs: dict[str, Any] = {
            "model": self.model,
            "capabilities": types.CapabilitiesConfig(
                agent_behavior=behavior,
                enable_subagents=True,
                max_subagent_depth=2,
            ),
        }
        if vertex_project:
            cfg_kwargs["vertex"] = True
            cfg_kwargs["project"] = vertex_project
            cfg_kwargs["location"] = os.getenv("GOOGLE_CLOUD_LOCATION", "us-central1")
        elif api_key:
            cfg_kwargs["api_key"] = api_key

        config = LocalAgentConfig(**cfg_kwargs)
        full_prompt = f"{system_instruction}\n\n{prompt}".strip() if system_instruction else prompt
        chunks: list[str] = []
        async with Agent(config=config) as agent:
            conv = agent.start_conversation()
            async for step in conv.chat(full_prompt):
                content = getattr(step, "content", None)
                if content:
                    chunks.append(str(content))
        return "".join(chunks).strip()

    def _ask_via_antigravity_cli(self, prompt: str, system_instruction: str = "") -> str:
        """Tier 2: Public `antigravity` or `agy` CLI binary on host/container."""
        agy_bin = shutil.which("antigravity") or shutil.which("agy")
        if not agy_bin:
            raise FileNotFoundError("Neither `antigravity` nor `agy` CLI found on PATH.")
        full_prompt = f"{system_instruction}\n\n{prompt}".strip() if system_instruction else prompt
        proc = subprocess.run(
            [agy_bin, "-p", full_prompt, "--model", self.model],
            cwd=self.repo_root,
            capture_output=True,
            text=True,
            timeout=120.0,
            check=False,
        )
        out = (proc.stdout or "").strip()
        if proc.returncode != 0 or not out:
            raise RuntimeError(f"antigravity CLI exited {proc.returncode}: {(proc.stderr or out)[:300]}")
        return out

    def _ask_via_rest(self, prompt: str, system_instruction: str = "") -> str:
        """Tier 3: Direct Argon / Gemini REST endpoint over HTTPS."""
        api_key = os.getenv("ARGON_API_KEY") or os.getenv("GEMINI_API_KEY")
        if not api_key:
            raise RuntimeError("No ARGON_API_KEY or GEMINI_API_KEY set for REST fallback.")
        base_url = os.getenv(
            "ARGON_ENDPOINT_URL",
            f"https://generativelanguage.googleapis.com/v1beta/models/{self.model}:generateContent",
        )
        url = f"{base_url}?key={api_key}" if "?" not in base_url else base_url
        payload: dict[str, Any] = {
            "contents": [{"role": "user", "parts": [{"text": prompt}]}],
            "generationConfig": {"temperature": 0.2},
        }
        if system_instruction:
            payload["systemInstruction"] = {"parts": [{"text": system_instruction}]}
        req = urllib.request.Request(
            url,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=90.0) as resp:
            body = json.loads(resp.read().decode("utf-8"))
        candidates = body.get("candidates") or []
        if not candidates:
            raise RuntimeError(f"Empty Argon REST response: {body}")
        parts = candidates[0].get("content", {}).get("parts") or []
        return "".join(str(p.get("text", "")) for p in parts).strip()

    def generate(self, prompt: str, system_instruction: str = "") -> dict[str, Any]:
        """Execute prompt against Argon via Antigravity SDK -> Antigravity CLI -> Argon REST."""
        errors: list[str] = []
        try:
            reply = asyncio.run(self._ask_via_sdk(prompt, system_instruction=system_instruction))
            if reply:
                return {"reply": reply, "backend": "google-antigravity-sdk", "model": self.model}
        except Exception as exc:
            errors.append(f"SDK: {exc}")

        try:
            reply = self._ask_via_antigravity_cli(prompt, system_instruction=system_instruction)
            if reply:
                return {"reply": reply, "backend": "antigravity-cli", "model": self.model}
        except Exception as exc:
            errors.append(f"CLI: {exc}")

        try:
            reply = self._ask_via_rest(prompt, system_instruction=system_instruction)
            if reply:
                return {"reply": reply, "backend": "argon-rest-api", "model": self.model}
        except Exception as exc:
            errors.append(f"REST: {exc}")

        raise RuntimeError(f"All Antigravity+Argon backends failed ({'; '.join(errors)})")


def extract_evolve_block(cpp_text: str) -> str:
    """Extract code between // EVOLVE-BLOCK-START and // EVOLVE-BLOCK-END."""
    m = re.search(
        r"(//\s*EVOLVE-BLOCK-START\b.*?//\s*EVOLVE-BLOCK-END)",
        cpp_text,
        flags=re.DOTALL,
    )
    if not m:
        raise ValueError("Missing // EVOLVE-BLOCK-START ... // EVOLVE-BLOCK-END markers.")
    return m.group(1)
