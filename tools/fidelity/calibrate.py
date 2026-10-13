#!/usr/bin/env python3
"""Validate fixed calibration expectations and honest hand-review precision."""
import argparse
import hashlib
import json
from pathlib import Path

# This acceptance list never participates in audit scoring or selection.
KNOWN = ['PM_Footsteps', 'PM_FootstepEvent', 'PM_UpdateLean', 'PM_WalkMove',
         'PM_CmdScale', 'Pmove', 'Item_List_Paint']


def coverage(audit, names):
    result = {}
    for name in names:
        direct = [r for r in audit['functions'] if r['reference']['name'] == name]
        helpers = [f for f in audit.get('native_only', []) if f['name'] == name]
        if direct:
            score = max(r.get('comparison', {}).get('score', 0) for r in direct)
            result[name] = dict(flagged=score >= 8, score=score, basis='direct-reference',
                                addresses=[r['reference'].get('address') for r in direct])
        else:
            signals = [s for f in helpers for s in f['ancestor_signals']]
            score = max((s['score'] for s in signals), default=0)
            result[name] = dict(flagged=score >= 8, score=score, basis='caller-context', ancestors=signals)
    return result


def precision(reviews):
    positive = sum(r['verdict'] == 'difference' for r in reviews)
    negative = sum(r['verdict'] == 'equivalent' for r in reviews)
    unknown = sum(r['verdict'] == 'unresolved' for r in reviews)
    if positive + negative + unknown != len(reviews):
        raise ValueError('Unknown review verdict')
    return dict(differences=positive, equivalent=negative, unresolved=unknown,
                precision_lower=positive / len(reviews) if reviews else None,
                precision_upper=(positive + unknown) / len(reviews) if reviews else None,
                resolved_precision=positive / (positive + negative) if positive + negative else None)


def evaluate(audit, positive_control=None):
    if positive_control is None:
        return dict(known_bugs=coverage(audit, KNOWN))
    if (not audit.get('source_commit') or not positive_control.get('source_commit') or
            not audit.get('binary_sha256') or
            audit['binary_sha256'] != positive_control.get('binary_sha256')):
        raise ValueError('Positive control requires source provenance and the same reference binary')
    return dict(known_bugs=coverage(positive_control, KNOWN),
                positive_control=dict(source_commit=positive_control['source_commit'],
                                      binary_sha256=positive_control['binary_sha256']),
                current_source_commit=audit['source_commit'],
                current_known_cases=coverage(audit, KNOWN))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('audit', type=Path)
    parser.add_argument('--reviews', type=Path)
    parser.add_argument('--positive-control', type=Path,
                        help='Archived audit of the known-bug source; current scores remain diagnostic')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    audit = json.loads(args.audit.read_text())
    control_bytes = args.positive_control.read_bytes() if args.positive_control else None
    result = evaluate(audit, json.loads(control_bytes) if control_bytes is not None else None)
    if control_bytes is not None:
        result['positive_control'].update(path=str(args.positive_control.resolve()),
                                          audit_sha256=hashlib.sha256(control_bytes).hexdigest())
    if args.reviews:
        reviews = json.loads(args.reviews.read_text())
        for group in ('flagged', 'unflagged'):
            expected = {(r['name'], r['address']) for r in audit['sample'][group]}
            actual = {(r['name'], r['address']) for r in reviews[group]}
            if expected != actual or len(reviews[group]) != 30 or any(not r.get('note') for r in reviews[group]):
                raise ValueError(group + ': reviews must exactly match the seeded 30-function sample with notes')
            result[group] = precision(reviews[group])
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if all(r['flagged'] for r in result['known_bugs'].values()) else 1


if __name__ == '__main__': raise SystemExit(main())
