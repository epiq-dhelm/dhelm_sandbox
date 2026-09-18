#!/usr/bin/env python3
# Inspect and update Bitbucket Cloud pull request review comments.
# The helper keeps the normal workflow read-only by default, reads credentials
# from .netrc, and intentionally exposes no merge, approve, or decline action.

from __future__ import annotations

import argparse
import base64
import json
import netrc
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple, cast

API_MACHINE = "api.bitbucket.org"
API_BASE_URL = "https://api.bitbucket.org/2.0"
DEFAULT_PAGELEN = 100

JsonMap = Mapping[str, object]
MutableJsonMap = Dict[str, object]


@dataclass(frozen=True)
class PullRequestRef:
    """Identify a Bitbucket Cloud pull request from its browser URL."""

    workspace: str
    repo_slug: str
    pull_request_id: int


class BitbucketClient:
    """Call the small subset of Bitbucket PR comment APIs this workflow needs."""

    def __init__(self, username: str, password: str) -> None:
        token = f"{username}:{password}".encode("utf-8")
        self._auth_header = "Basic " + base64.b64encode(token).decode("ascii")

    def get_json(self, url: str) -> JsonMap:
        """Fetch a JSON object from Bitbucket."""
        response = self._request_json("GET", url)
        if not isinstance(response, Mapping):
            raise RuntimeError(f"Expected JSON object from {url}")
        return cast(JsonMap, response)

    def post_json(
        self,
        url: str,
        payload: Optional[JsonMap] = None,
    ) -> Optional[object]:
        """POST an optional JSON payload and return the decoded response."""
        return self._request_json("POST", url, payload)

    def delete(self, url: str) -> Optional[object]:
        """DELETE a Bitbucket resource and return any decoded response body."""
        return self._request_json("DELETE", url)

    def _request_json(
        self,
        method: str,
        url: str,
        payload: Optional[JsonMap] = None,
    ) -> Optional[object]:
        """Send one authenticated request without ever printing credentials."""
        data = None
        headers = {
            "Accept": "application/json",
            "Authorization": self._auth_header,
        }
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"

        request = urllib.request.Request(
            url,
            data=data,
            headers=headers,
            method=method,
        )
        try:
            with urllib.request.urlopen(request) as response:
                body = response.read()
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(
                f"Bitbucket API {method} {url} failed with HTTP "
                f"{exc.code}: {detail}"
            ) from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(
                f"Bitbucket API {method} {url} failed: {exc.reason}"
            ) from exc

        if not body:
            return None
        return json.loads(body.decode("utf-8"))


def _load_netrc_auth(machine: str) -> Tuple[str, str]:
    """Read the Bitbucket login and API token from the user's .netrc file."""
    try:
        auth = netrc.netrc().authenticators(machine)
    except netrc.NetrcParseError as exc:
        raise RuntimeError(f"Could not parse .netrc: {exc}") from exc
    except FileNotFoundError as exc:
        raise RuntimeError("No .netrc file found for Bitbucket authentication") from exc

    if auth is None:
        raise RuntimeError(f"No .netrc entry found for machine {machine}")

    login, _, password = auth
    if not login or not password:
        raise RuntimeError(f"Incomplete .netrc entry for machine {machine}")
    return login, password


def _parse_pr_url(pr_url: str) -> PullRequestRef:
    """Parse a Bitbucket Cloud PR browser URL into API path components."""
    parsed = urllib.parse.urlparse(pr_url)
    parts = [part for part in parsed.path.split("/") if part]
    if len(parts) < 4 or parts[2] != "pull-requests":
        raise RuntimeError(
            "Expected Bitbucket PR URL like "
            "https://bitbucket.org/<workspace>/<repo>/pull-requests/<id>"
        )
    try:
        pull_request_id = int(parts[3])
    except ValueError as exc:
        raise RuntimeError(f"Invalid pull request id in URL: {parts[3]}") from exc

    return PullRequestRef(
        workspace=parts[0],
        repo_slug=parts[1],
        pull_request_id=pull_request_id,
    )


def _pr_api_url(pr_ref: PullRequestRef, suffix: str = "") -> str:
    """Build a Bitbucket Cloud pull request API URL."""
    quoted_workspace = urllib.parse.quote(pr_ref.workspace, safe="")
    quoted_repo = urllib.parse.quote(pr_ref.repo_slug, safe="")
    base = (
        f"{API_BASE_URL}/repositories/{quoted_workspace}/{quoted_repo}"
        f"/pullrequests/{pr_ref.pull_request_id}"
    )
    return f"{base}/{suffix.lstrip('/')}" if suffix else base


def _comment_api_url(pr_ref: PullRequestRef, comment_id: int) -> str:
    """Build the API URL for a specific PR comment."""
    return _pr_api_url(pr_ref, f"comments/{comment_id}")


def _paged_values(client: BitbucketClient, url: str) -> List[JsonMap]:
    """Read every page from a Bitbucket paginated list response."""
    values: List[JsonMap] = []
    next_url: Optional[str] = url
    while next_url:
        page = client.get_json(next_url)
        page_values = page.get("values", [])
        if not isinstance(page_values, list):
            raise RuntimeError(f"Expected paged values list from {next_url}")
        for item in page_values:
            if not isinstance(item, Mapping):
                raise RuntimeError(f"Expected JSON object in values from {next_url}")
            values.append(cast(JsonMap, item))

        raw_next = page.get("next")
        next_url = raw_next if isinstance(raw_next, str) else None
    return values


def _list_comments(client: BitbucketClient, pr_ref: PullRequestRef) -> List[JsonMap]:
    """Return all PR comments, including inline comments and replies."""
    query = urllib.parse.urlencode({"pagelen": DEFAULT_PAGELEN})
    return _paged_values(client, f"{_pr_api_url(pr_ref, 'comments')}?{query}")


def _is_unresolved_thread(comment: JsonMap) -> bool:
    """Return whether the comment is an unresolved top-level review thread."""
    if comment.get("deleted") is True:
        return False
    if isinstance(comment.get("parent"), Mapping):
        return False
    return not isinstance(comment.get("resolution"), Mapping)


def _nested_map(data: JsonMap, key: str) -> JsonMap:
    """Read a nested JSON object when Bitbucket returns one."""
    value = data.get(key, {})
    return cast(JsonMap, value) if isinstance(value, Mapping) else {}


def _string_field(data: JsonMap, key: str, default: str = "") -> str:
    """Read a string field from a dynamic JSON object."""
    value = data.get(key)
    return value if isinstance(value, str) else default


def _int_field(data: JsonMap, key: str) -> Optional[int]:
    """Read an integer field from a dynamic JSON object."""
    value = data.get(key)
    return value if isinstance(value, int) else None


def _comment_id(comment: JsonMap) -> int:
    """Return a PR comment id or fail if Bitbucket omitted it."""
    value = _int_field(comment, "id")
    if value is None:
        raise RuntimeError(f"Bitbucket comment is missing an integer id: {comment}")
    return value


def _comment_author(comment: JsonMap) -> str:
    """Return the reviewer's display name when Bitbucket provides it."""
    user = _nested_map(comment, "user")
    return _string_field(user, "display_name", "<unknown>")


def _comment_body(comment: JsonMap) -> str:
    """Return the raw Markdown text for a PR comment."""
    content = _nested_map(comment, "content")
    return _string_field(content, "raw")


def _comment_location(comment: JsonMap) -> str:
    """Return a PR-visible location string for inline comments."""
    inline = _nested_map(comment, "inline")
    path = _string_field(inline, "path")
    line = (
        _int_field(inline, "to")
        or _int_field(inline, "from")
        or _int_field(inline, "start_to")
        or _int_field(inline, "start_from")
    )
    if path and line is not None:
        return f"{path}:{line}"
    if path:
        return path
    return "<general PR comment>"


def _filter_comments(comments: Iterable[JsonMap], unresolved: bool) -> List[JsonMap]:
    """Apply the unresolved-thread filter requested by the CLI."""
    if not unresolved:
        return list(comments)
    return [comment for comment in comments if _is_unresolved_thread(comment)]


def _print_comments(comments: Sequence[JsonMap]) -> None:
    """Render PR comments in a compact text format for terminal review."""
    for comment in comments:
        print(f"Comment {_comment_id(comment)}")
        print(f"  Location: {_comment_location(comment)}")
        print(f"  Reviewer: {_comment_author(comment)}")
        print("  Text:")
        for line in _comment_body(comment).splitlines() or [""]:
            print(f"    {line}")
        print()


def _print_plan_template(comments: Sequence[JsonMap]) -> None:
    """Render a planning template that Codex fills in before code changes."""
    for comment in comments:
        print(f"### Comment {_comment_id(comment)}")
        print(f"- Location: {_comment_location(comment)}")
        print(f"- Reviewer: {_comment_author(comment)}")
        print("- Reviewer text:")
        for line in _comment_body(comment).splitlines() or [""]:
            print(f"  {line}")
        print("- Diagnosis:")
        print("- Recommended action:")
        print("- Proposed reply:")
        print("- Expected files/functions to change:")
        print()


def _read_message_file(path: Path) -> str:
    """Read Markdown content to use as a Bitbucket comment body."""
    message = path.read_text(encoding="utf-8").strip()
    if not message:
        raise RuntimeError(f"Message file is empty: {path}")
    return message


def _print_dry_run(action: str, url: str, payload: Optional[JsonMap] = None) -> None:
    """Show a write operation that was intentionally not sent."""
    print(f"DRY RUN: would {action}")
    print(f"URL: {url}")
    if payload is not None:
        print(json.dumps(payload, indent=2, sort_keys=True))


def _require_yes(args: argparse.Namespace, action: str) -> bool:
    """Return whether a mutating command is approved for execution."""
    if args.yes:
        return True
    print(f"Refusing to {action} without --yes.")
    print("Re-run with --yes only after the PR action has been approved.")
    return False


def _cmd_comments(args: argparse.Namespace, client: BitbucketClient) -> int:
    """List PR comments from Bitbucket."""
    pr_ref = _parse_pr_url(args.pr_url)
    comments = _filter_comments(_list_comments(client, pr_ref), args.unresolved)
    if args.json:
        print(json.dumps(comments, indent=2, sort_keys=True))
    else:
        _print_comments(comments)
    return 0


def _cmd_plan(args: argparse.Namespace, client: BitbucketClient) -> int:
    """Print a review-plan template for unresolved PR comments."""
    pr_ref = _parse_pr_url(args.pr_url)
    comments = _filter_comments(_list_comments(client, pr_ref), unresolved=True)
    _print_plan_template(comments)
    return 0


def _cmd_reply(args: argparse.Namespace, client: BitbucketClient) -> int:
    """Reply to an existing PR comment thread when explicitly approved."""
    pr_ref = _parse_pr_url(args.pr_url)
    message = _read_message_file(Path(args.message_file))
    url = _pr_api_url(pr_ref, "comments")
    payload: MutableJsonMap = {
        "content": {"raw": message},
        "parent": {"id": args.comment_id},
    }
    if not _require_yes(args, f"reply to comment {args.comment_id}"):
        _print_dry_run("post reply", url, payload)
        return 2
    response = client.post_json(url, payload)
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _cmd_comment(args: argparse.Namespace, client: BitbucketClient) -> int:
    """Post a general PR comment when explicitly approved."""
    pr_ref = _parse_pr_url(args.pr_url)
    message = _read_message_file(Path(args.message_file))
    url = _pr_api_url(pr_ref, "comments")
    payload: MutableJsonMap = {"content": {"raw": message}}
    if not _require_yes(args, "post a general PR comment"):
        _print_dry_run("post comment", url, payload)
        return 2
    response = client.post_json(url, payload)
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _cmd_resolve(args: argparse.Namespace, client: BitbucketClient) -> int:
    """Resolve an existing PR comment thread when explicitly approved."""
    pr_ref = _parse_pr_url(args.pr_url)
    url = f"{_comment_api_url(pr_ref, args.comment_id)}/resolve"
    if not _require_yes(args, f"resolve comment {args.comment_id}"):
        _print_dry_run("resolve comment thread", url)
        return 2
    response = client.post_json(url)
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _cmd_reopen(args: argparse.Namespace, client: BitbucketClient) -> int:
    """Reopen an existing PR comment thread when explicitly approved."""
    pr_ref = _parse_pr_url(args.pr_url)
    url = f"{_comment_api_url(pr_ref, args.comment_id)}/resolve"
    if not _require_yes(args, f"reopen comment {args.comment_id}"):
        _print_dry_run("reopen comment thread", url)
        return 2
    response = client.delete(url)
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _add_pr_url_arg(parser: argparse.ArgumentParser) -> None:
    """Add the common PR URL argument shared by all subcommands."""
    parser.add_argument("pr_url", help="Bitbucket pull request browser URL")


def _add_comment_id_arg(parser: argparse.ArgumentParser) -> None:
    """Add the common PR comment id argument for thread commands."""
    parser.add_argument("--comment-id", required=True, type=int)


def _add_message_file_arg(parser: argparse.ArgumentParser) -> None:
    """Add the common message-file argument for write commands."""
    parser.add_argument(
        "--message-file",
        required=True,
        help="Path to a UTF-8 Markdown file containing the comment text",
    )


def _add_yes_arg(parser: argparse.ArgumentParser) -> None:
    """Require an explicit flag before the helper mutates Bitbucket state."""
    parser.add_argument(
        "--yes",
        action="store_true",
        help="Actually send the write operation; without this the command is dry-run",
    )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse command-line arguments for PR comment inspection and updates."""
    parser = argparse.ArgumentParser(
        description=(
            "Inspect Bitbucket PR comments and perform explicitly approved "
            "comment-thread updates."
        )
    )
    parser.add_argument(
        "--machine",
        default=API_MACHINE,
        help="Machine name to read from .netrc",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    comments = subparsers.add_parser("comments", help="List PR comments")
    _add_pr_url_arg(comments)
    comments.add_argument(
        "--unresolved",
        action="store_true",
        help="Only show unresolved top-level review threads",
    )
    comments.add_argument("--json", action="store_true", help="Print raw JSON")
    comments.set_defaults(func=_cmd_comments)

    unresolved = subparsers.add_parser(
        "unresolved",
        help="List unresolved top-level PR review threads",
    )
    _add_pr_url_arg(unresolved)
    unresolved.add_argument("--json", action="store_true", help="Print raw JSON")
    unresolved.set_defaults(func=_cmd_comments, unresolved=True)

    plan = subparsers.add_parser(
        "plan",
        help="Print a review-plan template for unresolved comments",
    )
    _add_pr_url_arg(plan)
    plan.set_defaults(func=_cmd_plan)

    reply = subparsers.add_parser("reply", help="Reply to a PR comment thread")
    _add_pr_url_arg(reply)
    _add_comment_id_arg(reply)
    _add_message_file_arg(reply)
    _add_yes_arg(reply)
    reply.set_defaults(func=_cmd_reply)

    comment = subparsers.add_parser("comment", help="Post a general PR comment")
    _add_pr_url_arg(comment)
    _add_message_file_arg(comment)
    _add_yes_arg(comment)
    comment.set_defaults(func=_cmd_comment)

    resolve = subparsers.add_parser("resolve", help="Resolve a PR comment thread")
    _add_pr_url_arg(resolve)
    _add_comment_id_arg(resolve)
    _add_yes_arg(resolve)
    resolve.set_defaults(func=_cmd_resolve)

    reopen = subparsers.add_parser("reopen", help="Reopen a PR comment thread")
    _add_pr_url_arg(reopen)
    _add_comment_id_arg(reopen)
    _add_yes_arg(reopen)
    reopen.set_defaults(func=_cmd_reopen)

    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    """Run a Bitbucket PR helper subcommand."""
    args = parse_args(argv)
    username, password = _load_netrc_auth(args.machine)
    client = BitbucketClient(username, password)
    return cast(int, args.func(args, client))


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
