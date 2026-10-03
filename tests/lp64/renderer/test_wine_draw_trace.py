#!/usr/bin/env python3
"""Wine applies buffered constants inside DrawIndexedPrimitive, after its log."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


class WineDrawTrace(unittest.TestCase):
    def test_constants_are_captured_after_flush_on_calling_thread(self):
        root = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(prefix='cod2-wine-trace-') as directory:
            directory = Path(directory)
            log = directory / 'trace.log'
            log.write_text('''011c:trace:d3d9:d3d9_device_SetVertexShader iface 11111111, shader AAAA0001.
011c:trace:d3d9:d3d9_device_SetPixelShader iface 11111111, shader BBBB0001.
011c:trace:d3d9:d3d9_device_SetRenderState iface 11111111, state 0x1b, value 0.
011c:trace:d3d9:d3d9_device_DrawIndexedPrimitive iface 11111111, primitive_type 0x4, base_vertex_idx 0, min_vertex_idx 0, vertex_count 4, start_idx 0, primitive_count 2.
0120:trace:d3d:wined3d_device_set_ps_consts_f Set vec4 constant 0 to {99,99,99,99}.
011c:trace:d3d:wined3d_device_set_vs_consts_f Set vec4 constant 20 to {1,2,3,1}.
011c:trace:d3d:wined3d_device_set_ps_consts_f Set vec4 constant 0 to {16,16,0,0}.
0120:trace:d3d:wined3d_device_context_draw_indexed context 22222222, base_vertex_index 0, start_index 0, index_count 6, start_instance 0, instance_count 1.
011c:trace:d3d:wined3d_device_context_draw_indexed context 33333333, base_vertex_index 0, start_index 0, index_count 6, start_instance 0, instance_count 1.
011c:trace:d3d9:d3d9_device_SetPixelShader iface 11111111, shader BBBB0002.
011c:trace:d3d9:d3d9_device_SetRenderState iface 11111111, state 0x1b, value 1.
011c:trace:d3d9:d3d9_device_DrawIndexedPrimitive iface 11111111, primitive_type 0x4, base_vertex_idx 4, min_vertex_idx 0, vertex_count 4, start_idx 6, primitive_count 2.
011c:trace:d3d:wined3d_device_set_ps_consts_f Set vec4 constant 0 to {32,32,0,0}.
011c:trace:d3d:wined3d_device_context_draw_indexed context 33333333, base_vertex_index 4, start_index 6, index_count 6, start_instance 0, instance_count 1.
011c:trace:d3d9:d3d9_swapchain_Present iface 44444444.
''')
            subprocess.run(['python3', str(root / 'tools/macos-port/parse_wine_draws.py'),
                            str(log), str(directory / 'frames'), '--eye', 'view:1,2,3'],
                           check=True, capture_output=True, text=True)
            draws = [json.loads(line) for line in (directory / 'frames/view.jsonl').read_text().splitlines()]
            self.assertEqual(len(draws), 2)
            self.assertEqual(draws[0]['ps_constants'][:4], [16, 16, 0, 0])
            self.assertEqual(draws[1]['ps_constants'][:4], [32, 32, 0, 0])
            self.assertEqual(draws[0]['ps'], 'BBBB0001')
            self.assertEqual(draws[1]['ps'], 'BBBB0002')
            self.assertEqual(draws[0]['states']['27'], 0)
            self.assertEqual(draws[1]['states']['27'], 1)
            self.assertEqual(draws[0]['vs_constants'][80:84], [1, 2, 3, 1])


if __name__ == '__main__':
    unittest.main()
