"""Remove preprocessor blocks that depend on symbols whose value is known.

Works like unifdef -U (and -D) for sets of symbols, with three differences that this source
needs: directives continued over several lines are handled, an expression that is
only partly decided is rewritten without the decided part, and `#define` lines for
the symbols and `#include` lines for files that are not exported are dropped.
Lines that are not touched come out byte for byte.
"""
import re

DIRECTIVE = re.compile(r"^([ \t]*#[ \t]*)(if|ifdef|ifndef|elif|else|endif|define|undef|include)\b(.*)$", re.S)
TOKEN = re.compile(r"\s*(defined\b|[A-Za-z_]\w*|\d[\w.]*|&&|\|\||[()!]|==|!=|<=|>=|<<|>>|[-+*/%<>&|^~?:,]|'(?:\\.|[^'])*'|\"(?:\\.|[^\"])*\")")


class Unsupported(Exception):
    pass


def logical_lines(text):
    """Yield (raw, in_block_comment_at_start): raw keeps continuations and line ends."""
    lines = text.splitlines(keepends=True)
    in_comment = False
    i = 0
    while i < len(lines):
        start_in_comment = in_comment
        raw = lines[i]
        while raw.rstrip("\r\n").endswith("\\") and i + 1 < len(lines) and not start_in_comment \
                and raw.lstrip().startswith("#"):
            i += 1
            raw += lines[i]
        in_comment = scan(raw, in_comment)
        yield raw, start_in_comment
        i += 1


def scan(raw, in_comment):
    """Track block comments across a logical line, minding strings and // comments."""
    i, n = 0, len(raw)
    quote = None
    while i < n:
        c = raw[i]
        if in_comment:
            if raw.startswith("*/", i):
                in_comment = False
                i += 1
        elif quote:
            if c == "\\":
                i += 1
            elif c == quote or c == "\n":
                quote = None
        elif c in "\"'":
            quote = c
        elif raw.startswith("//", i):
            j = raw.find("\n", i)
            if j < 0:
                break
            i = j
        elif raw.startswith("/*", i):
            in_comment = True
            i += 1
        i += 1
    return in_comment


def strip_comments(expr):
    expr = re.sub(r"/\*.*?\*/", " ", expr, flags=re.S)
    expr = re.sub(r"/\*.*$", " ", expr, flags=re.S)
    expr = re.sub(r"//.*$", " ", expr, flags=re.M)
    return expr.replace("\\\r\n", " ").replace("\\\n", " ").strip()


class Node:
    def __init__(self, kind, text=None, value=None, changed=False):
        self.kind, self.text, self.value, self.changed = kind, text, value, changed


def simplify(expr, undefined, defined=frozenset()):
    """Return (value, text, changed): value is True, False or None when undecided."""
    tokens, pos = [], 0
    while pos < len(expr):
        m = TOKEN.match(expr, pos)
        if not m:
            if expr[pos:].strip() == "":
                break
            raise Unsupported(expr)
        tokens.append(m.group(1))
        pos = m.end()
    index = [0]

    def peek():
        return tokens[index[0]] if index[0] < len(tokens) else None

    def take():
        index[0] += 1
        return tokens[index[0] - 1]

    def parse_or():
        parts = [parse_and()]
        while peek() == "||":
            take()
            parts.append(parse_and())
        return combine(parts, "or")

    def parse_and():
        parts = [parse_unary()]
        while peek() == "&&":
            take()
            parts.append(parse_unary())
        return combine(parts, "and")

    def combine(parts, kind):
        if len(parts) == 1:
            return parts[0]
        decisive = kind == "or"
        if any(p.value is decisive for p in parts):
            return Node("atom", value=decisive, changed=True)
        kept = [p for p in parts if p.value is None]
        if not kept:
            return Node("atom", value=not decisive, changed=True)
        changed = len(kept) != len(parts) or any(p.changed for p in kept)
        if len(kept) == 1:
            return Node(kept[0].kind, kept[0].text, None, True)
        texts = [("(" + p.text + ")") if (kind == "and" and p.kind == "or") else p.text for p in kept]
        return Node(kind, (" || " if kind == "or" else " && ").join(texts), None, changed)

    def parse_unary():
        if peek() == "!":
            take()
            inner = parse_unary()
            if inner.value is not None:
                return Node("atom", value=not inner.value, changed=True)
            text = inner.text if inner.kind == "atom" else "(" + inner.text + ")"
            return Node("atom", "!" + text, None, inner.changed)
        return parse_primary()

    def parse_primary():
        tok = peek()
        if tok is None:
            raise Unsupported(expr)
        if tok == "defined":
            take()
            if peek() == "(":
                take(); name = take()
                if take() != ")":
                    raise Unsupported(expr)
            else:
                name = take()
            if name in undefined or name in defined:
                return Node("atom", value=name in defined, changed=True)
            return Node("atom", "defined(" + name + ")")
        start = index[0]
        if tok == "(":
            take()
            inner = parse_or()
            if take() != ")":
                raise Unsupported(expr)
            follows = peek()
            if follows in (None, "&&", "||", ")"):
                if inner.value is not None:
                    return Node("atom", value=inner.value, changed=True)
                if inner.kind == "atom":
                    return Node("atom", inner.text, None, inner.changed)
                return Node("atom", "(" + inner.text + ")", None, inner.changed)
            index[0] = start  # "(a) + b" and the like: read the whole run as one opaque atom
        depth, run = 0, []
        while peek() is not None:
            t = peek()
            if depth == 0 and t in ("&&", "||", ")"):
                break
            depth += t == "("
            depth -= t == ")"
            run.append(take())
        if len(run) == 1 and (run[0] in undefined or run[0] in defined):
            return Node("atom", value=run[0] in defined, changed=True)
        if any(t in undefined or t in defined for t in run):
            raise Unsupported(expr)
        text = ""
        for t in run:
            if text and (t[0].isalnum() or t[0] == "_") and (text[-1].isalnum() or text[-1] == "_"):
                text += " "
            elif text and t not in "(),)" and text[-1] not in "(!~" and not (t == "(" and (text[-1].isalnum() or text[-1] == "_")):
                text += " "
            text += t
        return Node("atom", text)

    node = parse_or()
    if peek() is not None:
        raise Unsupported(expr)
    return node.value, node.text, node.changed


def strip(text, undefined, drop_include=lambda name: False, where="", defined=frozenset()):
    out = []
    frames = []  # each: dict(state, on, emitted)

    def active():
        return all(f["on"] for f in frames)

    for raw, in_comment in logical_lines(text):
        m = None if in_comment else DIRECTIVE.match(raw)
        if not m:
            if active():
                out.append(raw)
            continue
        prefix, word, rest = m.groups()
        ending = "\r\n" if raw.endswith("\r\n") else "\n" if raw.endswith("\n") else ""
        if word in ("if", "ifdef", "ifndef"):
            if not active():
                frames.append({"state": "dead", "on": False, "emitted": False})
                continue
            value, line = evaluate(prefix, word, rest, raw, ending, undefined, where, defined=defined)
            if value is True:
                frames.append({"state": "taken", "on": True, "emitted": False})
            elif value is False:
                frames.append({"state": "pending", "on": False, "emitted": False})
            else:
                out.append(line)
                frames.append({"state": "open", "on": True, "emitted": True})
        elif word in ("elif", "else"):
            if not frames:
                raise Unsupported(f"{where}: #{word} without #if")
            f = frames[-1]
            if f["state"] == "dead":
                continue
            f["on"] = True  # judge the parents without this frame
            parents_on = all(g["on"] for g in frames[:-1])
            if f["state"] in ("taken", "closed"):
                f["on"] = False
                continue
            if word == "else":
                value, line = True, None
            else:
                value, line = evaluate(prefix, "if" if f["state"] == "pending" else "elif", rest, raw, ending, undefined, where,
                                       original_word="elif", defined=defined)
            if f["state"] == "pending":
                if value is True:
                    f["state"], f["on"] = "taken", True
                elif value is False:
                    f["on"] = False
                else:
                    if parents_on:
                        out.append(line)
                    f["state"], f["on"], f["emitted"] = "open", True, True
            else:  # open: an #if or #elif of this group is already in the output
                if value is True:
                    if parents_on:
                        out.append(raw if word == "else" else prefix + "else" + ending)
                    f["state"], f["on"] = "closed", True
                elif value is False:
                    f["on"] = False
                else:
                    if parents_on:
                        out.append(line)
                    f["on"] = True
        elif word == "endif":
            if not frames:
                raise Unsupported(f"{where}: #endif without #if")
            f = frames.pop()
            if f["emitted"] and active():
                out.append(raw)
        elif not active():
            continue
        elif word in ("define", "undef") and re.match(r"\s*([A-Za-z_]\w*)", rest) and \
                re.match(r"\s*([A-Za-z_]\w*)", rest).group(1) in (set(undefined) | set(defined)):
            continue
        elif word == "include" and re.match(r'\s*"([^"]+)"', rest) and drop_include(re.match(r'\s*"([^"]+)"', rest).group(1)):
            continue
        else:
            out.append(raw)
    if frames:
        raise Unsupported(f"{where}: #if without #endif")
    return "".join(out)


def evaluate(prefix, word, rest, raw, ending, undefined, where, original_word=None, defined=frozenset()):
    expr = strip_comments(rest)
    if word == "ifdef" or word == "ifndef":
        name = expr.split()[0] if expr.split() else ""
        if name in undefined or name in defined:
            return (word == "ifndef") != (name in defined), None
        return None, raw
    try:
        value, text, changed = simplify(expr, undefined, defined)
    except Unsupported:
        if any(re.search(r"\b" + re.escape(s) + r"\b", expr) for s in set(undefined) | set(defined)):
            raise Unsupported(f"{where}: cannot decide '{expr[:120]}'")
        return None, raw if original_word in (None, word) else prefix + word + rest
    if value is not None:
        return value, None
    if not changed and original_word in (None, word):
        return None, raw
    return None, prefix + word + " " + text + ending
