#!/usr/bin/env python3
"""Produce a reproducible, evidence-oriented delta report for QEMU RAFAELIA.

The script intentionally distinguishes source-level evidence from executable
evidence.  A changed file, a linked source, or a successful build must never be
silently promoted to a performance or guest-compatibility claim.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


REPORT_VERSION = 1
REMOTE_NAME = "audit-upstream-qemu"
REF_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._/-]*$")


class AuditError(RuntimeError):
    """An audit input was unavailable, so the result must be TOKEN_VAZIO."""


def run_git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        capture_output=True,
        text=True,
    )
    if check and result.returncode:
        command = "git " + " ".join(args)
        detail = (result.stderr or result.stdout).strip()
        raise AuditError(f"{command} failed ({result.returncode}): {detail}")
    return result


def git_text(repo: Path, *args: str) -> str:
    return run_git(repo, *args).stdout.strip()


def canonical_json(value: Any) -> str:
    return json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n"


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(canonical_json(value), encoding="utf-8")
    os.replace(temporary, path)


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(value, encoding="utf-8")
    os.replace(temporary, path)


def read_text(path: Path) -> str:
    if not path.is_file():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def validate_ref(ref: str) -> None:
    if not REF_RE.fullmatch(ref) or ".." in ref or ref.endswith("/"):
        raise AuditError(f"unsafe upstream ref: {ref!r}")


def ensure_upstream(repo: Path, upstream_url: str, upstream_ref: str, skip_fetch: bool) -> str:
    validate_ref(upstream_ref)
    remote_url = run_git(repo, "remote", "get-url", REMOTE_NAME, check=False)
    if remote_url.returncode:
        run_git(repo, "remote", "add", REMOTE_NAME, upstream_url)
    elif remote_url.stdout.strip() != upstream_url:
        run_git(repo, "remote", "set-url", REMOTE_NAME, upstream_url)

    remote_ref = f"{REMOTE_NAME}/{upstream_ref}"
    if not skip_fetch:
        run_git(
            repo,
            "fetch",
            "--no-tags",
            "--prune",
            REMOTE_NAME,
            f"+refs/heads/{upstream_ref}:refs/remotes/{REMOTE_NAME}/{upstream_ref}",
        )
    git_text(repo, "rev-parse", "--verify", remote_ref)
    return remote_ref


def parse_name_status(raw: str) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    for line in filter(None, raw.splitlines()):
        parts = line.split("\t")
        status = parts[0]
        if status[:1] in {"R", "C"} and len(parts) >= 3:
            entries.append({"status": status, "old_path": parts[1], "path": parts[2]})
        elif len(parts) >= 2:
            entries.append({"status": status, "path": parts[1]})
        else:
            entries.append({"status": status, "path": "TOKEN_VAZIO"})
    return entries


def parse_numstat(raw: str) -> dict[str, dict[str, str]]:
    values: dict[str, dict[str, str]] = {}
    for line in filter(None, raw.splitlines()):
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        path = parts[-1]
        values[path] = {"additions": parts[0], "deletions": parts[1]}
    return values


def classify_path(path: str) -> tuple[str, bool]:
    rules = (
        ("accel/tcg/", "tcg_execution_core", True),
        ("include/tcg/", "tcg_execution_core", True),
        ("target/", "target_cpu_model", True),
        ("system/", "system_lifecycle", True),
        ("hw/core/", "device_model_and_rafealia_runtime", True),
        ("qapi/", "qmp_api_surface", True),
        ("monitor/", "monitor_surface", True),
        ("qemu-options.hx", "command_line_surface", True),
        ("migration/", "migration_surface", True),
        ("block/", "block_surface", True),
        ("android/", "android_cross_build_contract", False),
        ("tools/rafaelia/", "artifact_and_audit_contract", False),
        (".github/workflows/", "ci_governance", False),
        ("docs/", "documentation_and_governance", False),
    )
    for prefix, surface, review_required in rules:
        if path == prefix or path.startswith(prefix):
            return surface, review_required
    return "other", False


def source_inventory(repo: Path) -> dict[str, Any]:
    meson_path = repo / "hw/core/meson.build"
    meson = read_text(meson_path)
    core_root = repo / "hw/core"
    sources: list[dict[str, Any]] = []
    if core_root.is_dir():
        for candidate in sorted(core_root.rglob("rafaelia*.c")):
            relative = candidate.relative_to(core_root).as_posix()
            sources.append(
                {
                    "path": f"hw/core/{relative}",
                    "listed_in_hw_core_meson": f"'{relative}'" in meson,
                }
            )

    checks = {
        "meson_registers_rafealia_runtime": "'rafaelia-runtime.c'" in meson,
        "meson_registers_rafealia_rmr": "'rafaelia-rmr.c'" in meson,
        "lifecycle_calls_runtime_init": "rafaelia_runtime_init" in read_text(repo / "system/vl.c"),
        "integration_makefile_present": (repo / "hw/core/Makefile.integration").is_file(),
        "artifact_contract_checker_present": (repo / "tools/rafaelia/check_qemu_artifact_contract.sh").is_file(),
    }
    return {"static_checks": checks, "rafaelia_sources": sources}


def parse_observations(raw_values: list[str]) -> dict[str, str]:
    observations: dict[str, str] = {}
    for raw in raw_values:
        if "=" not in raw:
            raise AuditError("--observation must be NAME=STATUS")
        name, status = raw.split("=", 1)
        name = name.strip()
        status = status.strip().upper()
        if not name or not re.fullmatch(r"[a-z0-9_]+", name):
            raise AuditError(f"invalid observation name: {name!r}")
        if status not in {"PASSED", "FAILED", "TOKEN_VAZIO"}:
            raise AuditError(f"invalid observation status for {name}: {status!r}")
        observations[name] = status
    return observations


def claim_status(observations: dict[str, str], key: str, fallback: str) -> str:
    observed = observations.get(key)
    if observed == "PASSED":
        return "OBSERVED_IN_CI"
    if observed == "FAILED":
        return "FAILED_IN_CI"
    if observed == "TOKEN_VAZIO":
        return "TOKEN_VAZIO"
    return fallback


def make_claims(inventory: dict[str, Any], observations: dict[str, str]) -> list[dict[str, str]]:
    checks = inventory["static_checks"]
    wiring = "STATIC_EVIDENCE" if checks["lifecycle_calls_runtime_init"] and checks["meson_registers_rafealia_runtime"] else "TOKEN_VAZIO"
    return [
        {
            "id": "qemu_upstream_build_path",
            "status": "STATIC_EVIDENCE" if checks["meson_registers_rafealia_runtime"] else "TOKEN_VAZIO",
            "evidence": "QEMU Meson build description is present; this is not a binary execution result.",
        },
        {
            "id": "rafaelia_runtime_wiring",
            "status": wiring,
            "evidence": "system/vl.c and hw/core/meson.build static inspection.",
        },
        {
            "id": "rafaelia_integration_self_test",
            "status": claim_status(observations, "integration_self_test", "TEST_DECLARED"),
            "evidence": "hw/core/Makefile.integration; a CI observation is required for executable proof.",
        },
        {
            "id": "qemu_system_binary_startup",
            "status": claim_status(observations, "qemu_system_binary", "TOKEN_VAZIO"),
            "evidence": "A qemu-system-* startup observation is required; source presence is insufficient.",
        },
        {
            "id": "tcg_hot_path_optimization",
            "status": "TOKEN_VAZIO",
            "evidence": "No benchmark plus controlled upstream comparison is supplied by this audit.",
        },
        {
            "id": "android_native_system_execution",
            "status": "TOKEN_VAZIO",
            "evidence": "Cross-compilation or host execution does not prove native Android system-mode execution.",
        },
        {
            "id": "performance_improvement",
            "status": "TOKEN_VAZIO",
            "evidence": "Requires reproducible benchmark inputs, environment, samples, and an upstream control.",
        },
    ]


def render_markdown(summary: dict[str, Any], delta: dict[str, Any], claims: list[dict[str, str]]) -> str:
    lines = [
        "# QEMU RAFAELIA — upstream functional audit",
        "",
        f"- Audit status: `{summary['status']}`",
        f"- Fork HEAD: `{summary.get('head', 'TOKEN_VAZIO')}`",
        f"- Upstream: `{summary.get('upstream', 'TOKEN_VAZIO')}`",
        f"- Merge base: `{summary.get('merge_base', 'TOKEN_VAZIO')}`",
        f"- Changed paths: `{delta.get('changed_path_count', 0)}`",
        "",
        "## Functional surfaces requiring review",
        "",
    ]
    surfaces = delta.get("surfaces", {})
    for surface in sorted(surfaces):
        item = surfaces[surface]
        marker = "yes" if item["review_required"] else "no"
        lines.append(f"- `{surface}`: {item['changed_paths']} path(s); manual review: `{marker}`")
    if not surfaces:
        lines.append("- `TOKEN_VAZIO`: no comparable delta was produced.")
    lines.extend(["", "## Claim evidence states", ""])
    for claim in claims:
        lines.append(f"- `{claim['id']}` — `{claim['status']}`. {claim['evidence']}")
    lines.extend(
        [
            "",
            "`TOKEN_VAZIO` is a deliberate absence-of-evidence marker, not a successful result.",
            "Source-level evidence and executable observations remain separate in the JSON artifacts.",
            "",
        ]
    )
    return "\n".join(lines)


def error_reports(out_dir: Path, error: str) -> None:
    summary = {
        "report_version": REPORT_VERSION,
        "status": "TOKEN_VAZIO",
        "error": error,
        "meaning": "The upstream comparison could not be established; no compatibility or performance conclusion is valid.",
    }
    delta = {"report_version": REPORT_VERSION, "status": "TOKEN_VAZIO", "error": error, "changed_path_count": 0, "paths": [], "surfaces": {}}
    claims = [
        {
            "id": "upstream_comparison",
            "status": "TOKEN_VAZIO",
            "evidence": "Upstream fetch, reference resolution, or merge-base calculation failed.",
        }
    ]
    inventory = {"report_version": REPORT_VERSION, "status": "TOKEN_VAZIO", "error": error}
    write_json(out_dir / "qemu_audit_summary.json", summary)
    write_json(out_dir / "qemu_upstream_delta.json", delta)
    write_json(out_dir / "qemu_capability_inventory.json", inventory)
    write_json(out_dir / "qemu_claims.json", claims)
    write_text(out_dir / "qemu_upstream_delta.md", render_markdown(summary, delta, claims))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="QEMU checkout to audit (default: current directory)")
    parser.add_argument("--upstream-url", default="https://github.com/qemu/qemu.git")
    parser.add_argument("--upstream-ref", default="master")
    parser.add_argument("--out-dir", default="artifacts/qemu-upstream-audit")
    parser.add_argument("--skip-fetch", action="store_true", help="Use an already-fetched audit upstream remote")
    parser.add_argument("--require-clean-tree", action="store_true")
    parser.add_argument("--fail-on-whitespace", action="store_true")
    parser.add_argument("--observation", action="append", default=[], metavar="NAME=STATUS")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    out_dir = Path(args.out_dir).resolve()
    try:
        observations = parse_observations(args.observation)
        git_text(repo, "rev-parse", "--is-inside-work-tree")
        remote_ref = ensure_upstream(repo, args.upstream_url, args.upstream_ref, args.skip_fetch)
        head = git_text(repo, "rev-parse", "HEAD")
        upstream = git_text(repo, "rev-parse", remote_ref)
        merge_base = git_text(repo, "merge-base", "HEAD", remote_ref)
        clean_status = git_text(repo, "status", "--porcelain=v1", "--untracked-files=all")
        name_status = parse_name_status(git_text(repo, "diff", "--name-status", "-M", merge_base, "HEAD"))
        numstat = parse_numstat(git_text(repo, "diff", "--numstat", "-M", merge_base, "HEAD"))
        diff_check = run_git(repo, "diff", "--check", merge_base, "HEAD", check=False)
        inventory = source_inventory(repo)

        surfaces: dict[str, dict[str, Any]] = {}
        paths: list[dict[str, Any]] = []
        for entry in name_status:
            surface, review_required = classify_path(entry["path"])
            surface_summary = surfaces.setdefault(
                surface, {"changed_paths": 0, "review_required": review_required}
            )
            surface_summary["changed_paths"] += 1
            surface_summary["review_required"] = surface_summary["review_required"] or review_required
            path_entry: dict[str, Any] = {
                **entry,
                **numstat.get(entry["path"], {"additions": "TOKEN_VAZIO", "deletions": "TOKEN_VAZIO"}),
                "surface": surface,
                "review_required": review_required,
            }
            paths.append(path_entry)

        delta = {
            "report_version": REPORT_VERSION,
            "status": "COMPARABLE",
            "base": merge_base,
            "head": head,
            "upstream": upstream,
            "changed_path_count": len(paths),
            "paths": paths,
            "surfaces": {key: surfaces[key] for key in sorted(surfaces)},
            "diff_check": {
                "status": "PASS" if diff_check.returncode == 0 else "REVIEW_REQUIRED",
                "output": (diff_check.stdout + diff_check.stderr).strip() or "TOKEN_VAZIO",
            },
        }
        claims = make_claims(inventory, observations)
        violations: list[str] = []
        if args.require_clean_tree and clean_status:
            violations.append("working tree is not clean")
        if args.fail_on_whitespace and diff_check.returncode:
            violations.append("git diff --check reported whitespace errors")
        summary = {
            "report_version": REPORT_VERSION,
            "status": "PASS" if not violations else "FAIL",
            "head": head,
            "upstream": upstream,
            "merge_base": merge_base,
            "head_commit_timestamp": git_text(repo, "show", "-s", "--format=%cI", "HEAD"),
            "upstream_ref": args.upstream_ref,
            "upstream_url": args.upstream_url,
            "changed_path_count": len(paths),
            "review_required_surfaces": sorted(key for key, value in surfaces.items() if value["review_required"]),
            "working_tree_clean_before_report": not bool(clean_status),
            "observations": observations,
            "gate_violations": violations,
            "evidence_model": {
                "OBSERVED_IN_CI": "A command was run by the workflow and reported PASSED.",
                "STATIC_EVIDENCE": "The source tree or build metadata contains the inspected evidence.",
                "TEST_DECLARED": "A test exists but this report has no successful execution observation.",
                "REVIEW_REQUIRED": "A change is comparable but requires human technical review.",
                "TOKEN_VAZIO": "No sufficient evidence was supplied; it is not a pass.",
            },
        }
        inventory["report_version"] = REPORT_VERSION
        inventory["status"] = "COMPARABLE"
        inventory["head"] = head
        inventory["upstream"] = upstream
        inventory["observations"] = observations

        write_json(out_dir / "qemu_audit_summary.json", summary)
        write_json(out_dir / "qemu_upstream_delta.json", delta)
        write_json(out_dir / "qemu_capability_inventory.json", inventory)
        write_json(out_dir / "qemu_claims.json", claims)
        write_text(out_dir / "qemu_upstream_delta.md", render_markdown(summary, delta, claims))
        print(f"QEMU upstream audit: {summary['status']} ({len(paths)} changed paths)")
        return 0 if not violations else 1
    except AuditError as error:
        error_reports(out_dir, str(error))
        print(f"QEMU upstream audit: TOKEN_VAZIO: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
