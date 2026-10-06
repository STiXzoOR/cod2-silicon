#!/usr/bin/env python3
"""Reject native release trace output that lacks the common opt-in gate.

This is a lexical guard check, not a C compiler. It follows native preprocessor
branches and requires a local runtime gate (or a reconstruction probe compiled
out by port_debug.h). Comments, sibling guards and unrelated environment flags
do not count. Unknown preprocessor conditions are scanned conservatively.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
NATIVE = {
    'COD2_X64': 1, 'COD2_PORT_DEBUG': 0, 'COD2_APPLE_SDK': 1,
    '__APPLE__': 1, '__aarch64__': 1, '__arm64__': 1,
    '_WIN32': 0, '_M_X64': 0, '__x86_64__': 0, '_DEBUG': 0,
    '__EMSCRIPTEN__': 0, 'PM_STUCKTRACE': 0, 'PM_CMDTRACE': 0, 'VM_TRACE': 0,
    'GL_NULLTRACE': 0, 'GFX_REAL_D3D9': 0, 'GFX_V60_ABI': 0,
}
LEX = re.compile(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|'
                 r"'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|\d+|&&|\|\||==|!=|<=|>=|[^\s]")
OUTPUT = {'Com_Printf', 'Com_DPrintf', 'printf', 'fprintf', 'Printf', 'DPrintf'}
TAG = re.compile(r"^(?:\\n)*(?:\[[A-Za-z][A-Za-z0-9_-]*\]|setClientCvar client=|"
                 r"client dvar command |script menu '%s': join dvars| target=%d| text='%\.80s')")
TRACE_FILE = re.compile(r'(?:x64_[^"/]*_trace\.txt|cod2_heapdiag\.txt)')


def pp_value(expression):
    """Return a known native boolean, or None for an unresolved condition."""
    expression = re.sub(r'defined\s*(?:\(\s*(\w+)\s*\)|(\w+))',
                        lambda m: str(NATIVE.get(m[1] or m[2], 'UNKNOWN')), expression)
    expression = re.sub(r'\b[A-Za-z_]\w*\b',
                        lambda m: str(NATIVE.get(m[0], 'UNKNOWN')), expression)
    expression = expression.strip()
    while expression.startswith('(') and expression.endswith(')'):
        depth = 0
        for i, char in enumerate(expression):
            depth += (char == '(') - (char == ')')
            if depth == 0:
                break
        if i != len(expression) - 1:
            break
        expression = expression[1:-1].strip()
    for operator in ('||', '&&'):
        depth = 0
        for i, char in enumerate(expression):
            depth += (char == '(') - (char == ')')
            if expression[i:i + 2] == operator and depth == 0:
                left, right = pp_value(expression[:i]), pp_value(expression[i + 2:])
                if operator == '&&':
                    return False if False in (left, right) else (True if left is True and right is True else None)
                return True if True in (left, right) else (False if left is False and right is False else None)
    if expression.startswith('!') and not expression.startswith('!='):
        value = pp_value(expression[1:])
        return None if value is None else not value
    if 'UNKNOWN' in expression or re.search(r'[^\d\s!&|()=<>]', expression):
        return None
    expression = expression.replace('&&', ' and ').replace('||', ' or ')
    expression = re.sub(r'!(?!=)', ' not ', expression)
    try:
        return bool(eval(expression, {'__builtins__': {}}, {}))
    except (SyntaxError, ValueError):
        return None


def native_source(source):
    """Blank inactive/directive lines without changing source coordinates."""
    output, stack = [], []
    active = True
    for line in source.splitlines(keepends=True):
        directive = re.match(r'\s*#\s*(\w+)\b(.*)', line)
        if directive:
            op, arg = directive.groups()
            if op in ('if', 'ifdef', 'ifndef'):
                expression = arg if op == 'if' else 'defined(' + arg.strip() + ')'
                value = pp_value(expression)
                if op == 'ifndef' and value is not None:
                    value = not value
                stack.append((active, value))
                active = active and value is not False
            elif op == 'else' and stack:
                parent, value = stack[-1]
                active = parent and value is not True
            elif op == 'elif' and stack:
                parent, prior = stack[-1]
                value = pp_value(arg)
                active = parent and prior is not True and value is not False
                stack[-1] = (parent, True if prior is True or value is True else None)
            elif op == 'endif' and stack:
                active = stack.pop()[0]
            output.append(re.sub(r'[^\n]', ' ', line))
        else:
            output.append(line if active else re.sub(r'[^\n]', ' ', line))
    return ''.join(output)


def scan_source(source, path='<source>'):
    source = native_source(source)
    tokens = [(m[0], m.start()) for m in LEX.finditer(source)
              if not m[0].startswith(('/*', '//'))]
    pairs, stack = {}, []
    for i, (value, _) in enumerate(tokens):
        if value in ('(', '{', '['):
            stack.append(i)
        elif value in (')', '}', ']') and stack:
            start = stack.pop()
            if tokens[start][0] == {')': '(', '}': '{', ']': '['}[value]:
                pairs[start] = i
    safe = set()

    def mark(start, end):
        safe.update(range(start, end))

    def statement_end(start):
        if start >= len(tokens):
            return start
        if tokens[start][0] == '{':
            return pairs.get(start, start) + 1
        i = start
        while i < len(tokens):
            if tokens[i][0] == ';':
                return i + 1
            if tokens[i][0] in ('(', '[', '{') and i in pairs:
                i = pairs[i] + 1
            else:
                i += 1
        return i

    def default_value(start, end):
        condition = [v for v, _ in tokens[start:end]]
        def evaluate(values):
            while values and values[0] == '(' and values[-1] == ')':
                depth = 0
                for j, v in enumerate(values):
                    depth += (v == '(') - (v == ')')
                    if depth == 0:
                        break
                if j != len(values) - 1:
                    break
                values = values[1:-1]
            for operator in ('||', '&&'):
                depth = 0
                for j, v in enumerate(values):
                    depth += (v == '(') - (v == ')')
                    if v == operator and depth == 0:
                        left, right = evaluate(values[:j]), evaluate(values[j + 1:])
                        if operator == '&&':
                            return False if False in (left, right) else (True if left is True and right is True else None)
                        return True if True in (left, right) else (False if left is False and right is False else None)
            if values and values[0] == '!':
                value = evaluate(values[1:])
                return None if value is None else not value
            if values and values[0] in ('COD2_DEBUG_ENV', 'COD2_NATIVE_TRACE_ENABLED') and len(values) > 2 and values[1] == '(':
                depth = 0
                for j in range(1, len(values)):
                    depth += (values[j] == '(') - (values[j] == ')')
                    if depth == 0:
                        break
                tail = values[j + 1:]
                if not tail:
                    return False
                if len(tail) == 2 and tail[1] in ('0', 'NULL'):
                    if tail[0] == '==':
                        return True
                    if tail[0] == '!=':
                        return False
            return None
        return evaluate(condition)

    for i, (value, _) in enumerate(tokens):
        if value in ('COD2_DEBUG_ONLY', 'COD2_NATIVE_TRACE_ONLY') and i + 1 in pairs:
            mark(i, pairs[i + 1] + 1)
        if value != 'if' or i + 1 not in pairs:
            continue
        close = pairs[i + 1]
        start = close + 1
        end = statement_end(start)
        condition = default_value(i + 2, close)
        if condition is False:
            mark(start, end)
        # A gate early return protects only the rest of its containing block.
        body = [v for v, _ in tokens[start:end] if v not in ('{', '}')]
        if condition is True and body and body[0] == 'return' and body.count(';') == 1:
            enclosing = [b for b, e in pairs.items()
                         if tokens[b][0] == '{' and b < i < e]
            if enclosing:
                mark(end, pairs[max(enclosing)])

    findings = []
    for i, (value, position) in enumerate(tokens):
        if value not in OUTPUT | {'fopen'} or i + 1 not in pairs or i in safe:
            continue
        close = pairs[i + 1]
        strings, adjacent = [], False
        for token, _ in tokens[i + 2:close]:
            if token.startswith('"'):
                if adjacent:
                    strings[-1] += token[1:-1]
                else:
                    strings.append(token[1:-1])
                adjacent = True
            else:
                adjacent = False
        match = next((TRACE_FILE.search(s) if value == 'fopen' else TAG.search(s)
                      for s in strings if (TRACE_FILE.search(s) if value == 'fopen' else TAG.search(s))), None)
        if match:
            line = source.count('\n', 0, position) + 1
            findings.append(f'{path}:{line}: unguarded {"trace file" if value == "fopen" else "trace tag"} {match[0]}')
    return findings


def scan_paths(paths):
    findings = []
    for path in paths:
        findings.extend(scan_source(path.read_text(errors='replace'), path))
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='*', type=Path, help='source files; default: repository src/')
    args = parser.parse_args()
    paths = args.paths or sorted(p for p in (ROOT / 'src').rglob('*')
                                if p.suffix in ('.c', '.h', '.m', '.mm', '.cpp'))
    findings = scan_paths(paths)
    for finding in findings:
        print(finding)
    print(f'native trace guards: {len(findings)} unguarded diagnostics in {len(paths)} files')
    return 1 if findings else 0


if __name__ == '__main__':
    raise SystemExit(main())
