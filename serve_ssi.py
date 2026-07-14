#!/usr/bin/env python3
"""
Small local web server with basic Apache-style Server Side Includes (SSI).

Supported directives:
    <!--#include virtual="/path/file.html" -->
    <!--#set var="name" value="value" -->
    <!--#if expr="..." -->
    <!--#elif expr="..." -->
    <!--#else -->
    <!--#endif -->

Supported expressions:
    "$name"
    "!$name"
    "$name = value"
    "$name == value"
    "$name != value"
    "$name =~ /regular expression/"
    "$name !~ /regular expression/"
    expr && expr
    expr || expr
    !expr
    (expr)

Variables may be written as $name or ${name}. Variable references in #set values,
include paths, and ordinary page text are not expanded; they are expanded only
inside SSI directive attributes and expressions.

This is intended for trusted local development only.
"""

from __future__ import annotations

import argparse
import html
import mimetypes
import os
import re
import sys
from dataclasses import dataclass
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit


DIRECTIVE_RE = re.compile(r"<!--\s*#(\w+)\b(.*?)-->", re.IGNORECASE | re.DOTALL)
ATTRIBUTE_RE = re.compile(
    r"""([A-Za-z_][A-Za-z0-9_-]*)\s*=\s*(?:"([^"]*)"|'([^']*)')""",
    re.DOTALL,
)
VARIABLE_RE = re.compile(r"\$(?:\{([A-Za-z_][A-Za-z0-9_]*)\}|([A-Za-z_][A-Za-z0-9_]*))")


class SSIError(Exception):
    pass


def parse_attributes(text: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for match in ATTRIBUTE_RE.finditer(text):
        key = match.group(1).lower()
        value = match.group(2) if match.group(2) is not None else match.group(3)
        result[key] = value
    return result


def expand_variables(text: str, variables: dict[str, str]) -> str:
    def replace(match: re.Match[str]) -> str:
        name = match.group(1) or match.group(2)
        return variables.get(name, "")
    return VARIABLE_RE.sub(replace, text)


def strip_quotes(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        return value[1:-1]
    return value


def find_top_level_operator(expr: str, operator: str) -> int:
    depth = 0
    quote: str | None = None
    regex_mode = False
    escape = False
    i = 0

    while i <= len(expr) - len(operator):
        ch = expr[i]

        if escape:
            escape = False
            i += 1
            continue

        if ch == "\\":
            escape = True
            i += 1
            continue

        if quote:
            if ch == quote:
                quote = None
            i += 1
            continue

        if regex_mode:
            if ch == "/":
                regex_mode = False
            i += 1
            continue

        if ch in "\"'":
            quote = ch
            i += 1
            continue

        if ch == "/":
            regex_mode = True
            i += 1
            continue

        if ch == "(":
            depth += 1
            i += 1
            continue

        if ch == ")":
            depth -= 1
            i += 1
            continue

        if depth == 0 and expr.startswith(operator, i):
            return i

        i += 1

    return -1


def has_outer_parentheses(expr: str) -> bool:
    if not (expr.startswith("(") and expr.endswith(")")):
        return False

    depth = 0
    quote: str | None = None
    escape = False

    for i, ch in enumerate(expr):
        if escape:
            escape = False
            continue
        if ch == "\\":
            escape = True
            continue
        if quote:
            if ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0 and i != len(expr) - 1:
                return False
    return depth == 0


def evaluate_expression(expr: str, variables: dict[str, str]) -> bool:
    expr = expr.strip()

    while has_outer_parentheses(expr):
        expr = expr[1:-1].strip()

    pos = find_top_level_operator(expr, "||")
    if pos >= 0:
        return evaluate_expression(expr[:pos], variables) or evaluate_expression(
            expr[pos + 2 :], variables
        )

    pos = find_top_level_operator(expr, "&&")
    if pos >= 0:
        return evaluate_expression(expr[:pos], variables) and evaluate_expression(
            expr[pos + 2 :], variables
        )

    if expr.startswith("!"):
        return not evaluate_expression(expr[1:], variables)

    expanded = expand_variables(expr, variables).strip()

    comparison_patterns = [
        (r"^(.*?)\s*!~\s*/(.*)/\s*$", "!~"),
        (r"^(.*?)\s*=~\s*/(.*)/\s*$", "=~"),
        (r"^(.*?)\s*!=\s*(.*?)\s*$", "!="),
        (r"^(.*?)\s*==\s*(.*?)\s*$", "=="),
        (r"^(.*?)\s*=\s*(.*?)\s*$", "="),
    ]

    for pattern, operator in comparison_patterns:
        match = re.match(pattern, expanded, re.DOTALL)
        if not match:
            continue

        left = strip_quotes(match.group(1))
        right = match.group(2)

        if operator in ("=~", "!~"):
            try:
                matched = re.search(right, left) is not None
            except re.error as exc:
                raise SSIError(f"Invalid regular expression in expr: {exc}") from exc
            return not matched if operator == "!~" else matched

        right = strip_quotes(right)
        if operator == "!=":
            return left != right
        return left == right

    value = strip_quotes(expanded)
    return value not in ("", "0", "false", "False", "no", "No")


@dataclass
class ConditionalFrame:
    parent_active: bool
    branch_taken: bool
    active: bool


class SSIProcessor:
    def __init__(self, document_root: Path, max_include_depth: int = 20):
        self.document_root = document_root.resolve()
        self.max_include_depth = max_include_depth

    def resolve_virtual_path(self, virtual_path: str, current_file: Path) -> Path:
        split = urlsplit(virtual_path)
        path_text = unquote(split.path)

        if path_text.startswith("/"):
            candidate = self.document_root / path_text.lstrip("/")
        else:
            candidate = current_file.parent / path_text

        candidate = candidate.resolve()

        try:
            candidate.relative_to(self.document_root)
        except ValueError as exc:
            raise SSIError(f"Include leaves document root: {virtual_path}") from exc

        return candidate

    def process_file(
        self,
        filename: Path,
        variables: dict[str, str] | None = None,
        depth: int = 0,
    ) -> str:
        if depth > self.max_include_depth:
            raise SSIError("Maximum SSI include depth exceeded")

        filename = filename.resolve()

        try:
            filename.relative_to(self.document_root)
        except ValueError as exc:
            raise SSIError("Requested file is outside the document root") from exc

        if not filename.is_file():
            raise SSIError(f"File not found: {filename.relative_to(self.document_root)}")

        try:
            text = filename.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = filename.read_text(encoding="latin-1")

        local_variables = dict(variables or {})
        local_variables.setdefault("DOCUMENT_URI", "/" + str(filename.relative_to(self.document_root)))
        local_variables.setdefault("DOCUMENT_NAME", filename.name)
        local_variables.setdefault("LAST_MODIFIED", str(int(filename.stat().st_mtime)))

        return self.process_text(text, filename, local_variables, depth)

    def process_text(
        self,
        text: str,
        current_file: Path,
        variables: dict[str, str],
        depth: int,
    ) -> str:
        output: list[str] = []
        frames: list[ConditionalFrame] = []
        cursor = 0

        def current_active() -> bool:
            return all(frame.active for frame in frames)

        for match in DIRECTIVE_RE.finditer(text):
            if current_active():
                output.append(text[cursor : match.start()])

            directive = match.group(1).lower()
            attributes = parse_attributes(match.group(2))

            if directive == "if":
                parent_active = current_active()
                condition = False
                if parent_active:
                    condition = evaluate_expression(attributes.get("expr", ""), variables)
                frames.append(
                    ConditionalFrame(
                        parent_active=parent_active,
                        branch_taken=condition,
                        active=parent_active and condition,
                    )
                )

            elif directive == "elif":
                if not frames:
                    raise SSIError("#elif without matching #if")
                frame = frames[-1]
                if not frame.parent_active or frame.branch_taken:
                    frame.active = False
                else:
                    condition = evaluate_expression(attributes.get("expr", ""), variables)
                    frame.active = condition
                    frame.branch_taken = condition

            elif directive == "else":
                if not frames:
                    raise SSIError("#else without matching #if")
                frame = frames[-1]
                frame.active = frame.parent_active and not frame.branch_taken
                frame.branch_taken = True

            elif directive == "endif":
                if not frames:
                    raise SSIError("#endif without matching #if")
                frames.pop()

            elif current_active():
                if directive == "include":
                    virtual_path = attributes.get("virtual")
                    if virtual_path is None:
                        raise SSIError('#include requires virtual="..."')
                    virtual_path = expand_variables(virtual_path, variables)
                    included_file = self.resolve_virtual_path(virtual_path, current_file)
                    output.append(
                        self.process_file(
                            included_file,
                            variables=variables,
                            depth=depth + 1,
                        )
                    )

                elif directive == "set":
                    name = attributes.get("var")
                    if not name:
                        raise SSIError('#set requires var="..."')
                    value = expand_variables(attributes.get("value", ""), variables)
                    variables[name] = value

                elif directive == "echo":
                    name = attributes.get("var", "")
                    output.append(html.escape(variables.get(name, "")))

                else:
                    output.append(match.group(0))

            cursor = match.end()

        if frames:
            raise SSIError("Unclosed #if block")

        if current_active():
            output.append(text[cursor:])

        return "".join(output)


class SSIRequestHandler(SimpleHTTPRequestHandler):
    server_version = "PythonSSI/1.1"

    def __init__(self, *args, directory: str | None = None, **kwargs):
        self.document_root = Path(directory or os.getcwd()).resolve()
        self.processor = SSIProcessor(self.document_root)
        super().__init__(*args, directory=str(self.document_root), **kwargs)

    def do_GET(self) -> None:
        self._serve(send_body=True)

    def do_HEAD(self) -> None:
        self._serve(send_body=False)

    def _serve(self, send_body: bool) -> None:
        request_path = unquote(urlsplit(self.path).path)
        filesystem_path = Path(self.translate_path(request_path))

        if filesystem_path.is_dir():
            # Redirect directory requests to their canonical trailing-slash URL.
            # Browsers otherwise resolve relative links against the parent path.
            split = urlsplit(self.path)
            if not split.path.endswith("/"):
                location = split.path + "/"
                if split.query:
                    location += "?" + split.query
                self.send_response(HTTPStatus.FOUND)
                self.send_header("Location", location)
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return

            for index_name in ("index.html", "index.htm", "index.shtml"):
                candidate = filesystem_path / index_name
                if candidate.is_file():
                    filesystem_path = candidate
                    break
            else:
                return super().do_GET() if send_body else super().do_HEAD()

        if filesystem_path.suffix.lower() not in (".html", ".htm", ".shtml"):
            return super().do_GET() if send_body else super().do_HEAD()

        try:
            relative_path = filesystem_path.resolve().relative_to(self.document_root)
            processed = self.processor.process_file(
                filesystem_path,
                variables={
                    "DOCUMENT_URI": "/" + relative_path.as_posix(),
                    "DOCUMENT_NAME": filesystem_path.name,
                    "QUERY_STRING_UNESCAPED": urlsplit(self.path).query,
                },
            )
            data = processed.encode("utf-8")
        except FileNotFoundError:
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return
        except SSIError as exc:
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR, str(exc))
            return
        except OSError as exc:
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR, str(exc))
            return

        content_type = mimetypes.guess_type(str(filesystem_path))[0] or "text/html"
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        if send_body:
            self.wfile.write(data)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Serve a directory with a small subset of Apache SSI."
    )
    parser.add_argument(
        "directory",
        nargs="?",
        default=".",
        help="document root; defaults to the current directory",
    )
    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="address to bind to; default: 127.0.0.1",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8000,
        help="port to use; default: 8000",
    )
    args = parser.parse_args()

    document_root = Path(args.directory).resolve()
    if not document_root.is_dir():
        parser.error(f"Not a directory: {document_root}")

    def handler(*handler_args, **handler_kwargs):
        return SSIRequestHandler(
            *handler_args,
            directory=str(document_root),
            **handler_kwargs,
        )

    server = ThreadingHTTPServer((args.host, args.port), handler)
    print("PythonSSI development server version 1.1")
    print(f"Serving {document_root}")
    print(f"Open http://{args.host}:{args.port}/")
    print("Press Ctrl-C to stop.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
