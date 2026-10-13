"""Independent, closed-form expectations for synthetic compatibility probes."""
import math


# Each history and both sets of pitch taps are constant. The interpolated raw
# comb output is therefore constant: 0.3, 37.5, 93.75, or 132.5. The last two
# follow from weighted tap magnitudes 1.25 and 2.5 and comb gain 0.25.
# RMS includes the original helper's 0.1 mean-square floor. Its gain is the
# smaller RMS divided by (output RMS + 1), with a lower limit of 0.5.
# Smoothing has the closed form gain + (initial - gain) * 0.96**sample.
COMB_CASES = (
    (0.3, 0.3, 1.0, 0.5),
    (37.5, 37.5, 1.0, math.sqrt(37.5**2 + 0.1) / (math.sqrt(37.5**2 + 0.1) + 1)),
    (100.0, 93.75, 0.8, math.sqrt(93.75**2 + 0.1) / (math.sqrt(93.75**2 + 0.1) + 1)),
    (100.0, 132.5, 0.8, math.sqrt(100**2 + 0.1) / (math.sqrt(132.5**2 + 0.1) + 1)),
)


def comb_expected(case):
    _, raw, initial, gain = COMB_CASES[case]
    return [raw * (gain + (initial - gain) * 0.96**sample) for sample in range(1, 41)]


# Hand-decoded pitch averages for the first three independently encoded
# mode-6 packets. They use the published Xiph 1.0.4 gain_cdbk_nb, with each
# signed entry converted by entry/64 + 0.5. These are synthetic packet
# expectations, not original binary table values or decoded original PCM.
# The independently read seven-bit indices are (0, 41, 29, 61),
# (95, 95, 59, 106), and (45, 29, 120, 127): bit 47 + 81*subframe.
# Average |center| + positive outer taps + half the magnitude of negative
# outer taps. These values govern subsequent concealment and recovery.
MODE6_PITCH_GAINS = (0.49609375, 0.947265625, 0.970703125)


def compatibility_checks(rows):
    checks = []
    comb = [row for row in rows if row.get('contract') == 'comb']
    if len(comb) != 4:
        raise ValueError('Expected four comb helper contract results')
    for row in comb:
        expected = comb_expected(row['case'])
        error = max(abs(a - b) for a, b in zip(row['pcm'], expected))
        checks.append(dict(name=f"comb RMS/tap contract {row['case']}",
                           passed=len(row['pcm']) == 40 and error <= 0.0003, max_abs=error))
    pitch = [row for row in rows if 'pitch_gain' in row]
    if len(pitch) != 3:
        raise ValueError('Expected three mode-6 pitch history results')
    for row, expected in zip(pitch, MODE6_PITCH_GAINS):
        checks.append(dict(name=f"mode 6 pitch history frame {row['frame']}",
                           passed=row['result'] == 0 and row['pitch_gain'] == expected,
                           actual=row['pitch_gain'], expected=expected))
    return checks
