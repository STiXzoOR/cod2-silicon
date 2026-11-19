"""Extract one function's full text from production C source."""
import re


def function(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError(f'missing function: {name}')
    depth = 0
    tokens = re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():])
    for token in tokens:
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError(f'unterminated function: {name}')
